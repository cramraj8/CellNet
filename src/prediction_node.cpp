//
//	Copyright (c) 2015, Emory University
//	All rights reserved.
//
//	Redistribution and use in source and binary forms, with or without modification, are
//	permitted provided that the following conditions are met:
//
//	1. Redistributions of source code must retain the above copyright notice, this list of
//	conditions and the following disclaimer.
//
//	2. Redistributions in binary form must reproduce the above copyright notice, this list
// 	of conditions and the following disclaimer in the documentation and/or other materials
//	provided with the distribution.
//
//	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
//	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
//	SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
//	TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
//	WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//	DAMAGE.
//
//
#include "prediction_node.h"


using namespace std;





PredictionNode::PredictionNode(string id, int mode, int batch_size, string test_model_path, 
							   string params_file, int device_id, string outFilename) : 
Node(id, mode), 
_batch_size(batch_size), 
_data_buffer(), 
_labels_buffer(), 
_predictions(), 
_test_model_path(test_model_path),  
_net(), 
_params_file(params_file), 
_device_id(device_id),
_outFilename(outFilename),
_out_layer(),
_data_layer()
{

	runtime_total_first = utils::get_time();
	_data_buffer.clear();
}





void PredictionNode::init_model()
{
	// Set gpu
	caffe::Caffe::SetDevice(_device_id);
    caffe::Caffe::set_mode(caffe::Caffe::GPU);
//    caffe::Caffe::DeviceQuery();
    
	cout << _id << " using GPU device " << _device_id << endl;

    // Create Net
    _net = boost::make_shared<caffe::Net<float>>(_test_model_path.c_str(), caffe::TEST);

    // Initialize the history
  	const vector<boost::shared_ptr<caffe::Blob<float> > >& net_params = _net->params();
  	_net->CopyTrainedLayersFrom(_params_file.c_str());

  	_data_layer = boost::static_pointer_cast<caffe::MemoryDataLayer<float>>(_net->layer_by_name("data"));
	_out_layer = _net->blob_by_name("prob");

  	cout << _id << ": Model loaded" << endl;
}






void *PredictionNode::run()
{

	double 	start = utils::get_time();
	int first_idx = 0;

	increment_threads();

	// Need to initialize the model in the running thread. Constructor is called
	// from another thread.
	//
	init_model();


	while(true){
		copy_chunk_from_buffer(_data_buffer, _labels_buffer);
		if(first_idx + _batch_size <= _data_buffer.size()){

			// For each epoch feed the model with a mini-batch of samples
			int epochs = (_data_buffer.size() - first_idx)/_batch_size;
			for(int i = 0; i < epochs; i++){	

				first_idx = step(first_idx, _batch_size);
			}	
		}
		else{

			// Check if all input nodes have already finished
			bool is_all_done = true;
			for(vector<int>::size_type i=0; i < _in_edges.size(); i++){

				if(!_in_edges.at(i)->is_in_node_done()){
					is_all_done = false;
				}
			}

			// All input nodes have finished
			if(is_all_done){

				// Handle non-multiples
				if(first_idx != _data_buffer.size()-1){
					
					cout << "Remaining samples: " << to_string(_data_buffer.size()-first_idx) << endl; 
					step(first_idx, _data_buffer.size()-first_idx);
				}

				// Print results
//				compute_accuracy();
//				print_out_labels();
				write_to_file();
				break;
			}
		}
	}

	if(check_finished() == true){

		cout << "******************" << endl 
			 << "Prediction" << endl 
			 << "Total_time_first: " << to_string(utils::get_time() - runtime_total_first) << endl 
			 << "# of elements: " << to_string(_labels_buffer.size()) << endl 
			 << "******************" << endl;
		cout << "PredictionNode runtime: " << utils::get_time() - start << endl;

		// Notify it has finished
		for(vector<int>::size_type i=0; i < _out_edges.size(); i++){
			_out_edges.at(i)->set_in_node_done();
		}
	}

	return NULL;
}





int PredictionNode::step(int first_idx, int batch_size)
{

	// Split batch
	vector<cv::Mat> batch;
	vector<int> batch_labels;

	// Reserve space
	batch.reserve(batch_size);
	batch.insert(batch.end(), _data_buffer.begin() + first_idx, _data_buffer.begin() + first_idx + batch_size);
	batch_labels.reserve(batch_size);
	batch_labels.insert(batch_labels.end(), _labels_buffer.begin() + first_idx, _labels_buffer.begin() + first_idx + batch_size);
	
	// Set batch size
	_data_layer->set_batch_size(batch_size);

	// Add matrices
	_data_layer->AddMatVector(batch, batch_labels);

	// Foward
	float loss;
	_net->ForwardPrefilled(&loss);

	const float* results = _out_layer->cpu_data();

	// Append outputs
	for(int j=0; j < _out_layer->shape(0); j++){
		
		int idx_max = 0;
		float max = results[j*_out_layer->shape(1) + 0], prob = 0.0f;
		
		// Argmax
//		for(int k=0; k < _out_layer->shape(1); k++){

//			if(results[j*_out_layer->shape(1) + k] > max){

//				max = results[j*_out_layer->shape(1) + k];
//				idx_max = k;
//			}
//		}

//		_predictions.push_back(idx_max);

		prob = results[j * _out_layer->shape(1) + 1];
		_predictions.push_back(prob);
	}
	first_idx += batch_size;

	// Clean variables
	batch.clear();
	batch_labels.clear();

	return first_idx;
}






void PredictionNode::compute_accuracy()
{

	int hit = 0;
	for(int i=0; i < _predictions.size(); i++){

		if(_labels_buffer[i] == _predictions[i]){

			hit++;
		}
	}

	float acc = (float)hit/_predictions.size();
	cout << "Accuracy: " << acc <<  " Hits: " << hit << endl;
}





void PredictionNode::write_to_file()
{

	ofstream out;

	out.open(_outFilename, ofstream::out  | ios::app);

	for(int i=0; i < _predictions.size(); i++){

		 out <<  _predictions[i] << ";" << _labels_buffer[i] << endl;
	}
	out.close();
}





void PredictionNode::print_out_labels()
{

	for(int i=0; i < _predictions.size(); i++){

		cout << "out: " << _predictions[i] << " target: " << _labels_buffer[i] << endl;
		vector<cv::Mat> input;
		split(_data_buffer[i], input);
//		for(int k = 0; k < input.size(); k++){

//			cv::imwrite("/home/nelson/CellNet/src/teste/img" + std::to_string(i)+std::to_string(k) + ".jpg", input[k]);
//		}
	}
}
