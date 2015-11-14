#define PI 3.1415926535897932
#include <cmath>
#include "augmentation_node.h"
#include "edge.h"
#include "utils.h"
#include <random>
#include <chrono>

AugmentationNode::AugmentationNode(std::string id, int mode, int aug_factor): Node(id, mode), _counter(0), _data_buffer(), _labels_buffer(), _aug_factor(aug_factor){
	runtime_total_first = utils::get_time();
	_data_buffer.clear();
	_labels_buffer.clear();
}

void *AugmentationNode::run(){

	while(true){

		copy_chunk_from_buffer(_data_buffer, _labels_buffer);
		if(!_data_buffer.empty()){
		
			// Augment data
			augment_images(_data_buffer, _labels_buffer);

			// Clean buffers
			_data_buffer.clear();
			_labels_buffer.clear();
		}
		else{

			// Check if all input nodes have already finished
			bool is_all_done = true;

			for(std::vector<int>::size_type i=0; i < _in_edges.size(); i++){

				if(!_in_edges.at(i)->is_in_node_done()){
					is_all_done = false;
				}
			}			

			// All input nodes have finished
			if(is_all_done){
				std::cout << "******************" << std::endl << "AugmentationNode" << std::endl << "Total_time_first: " << std::to_string(utils::get_time() - runtime_total_first) << std::endl << "# of elements: " << std::to_string(_counter) << std::endl << "******************" << std::endl;
				
				// Notify it has finished
				for(std::vector<int>::size_type i=0; i < _out_edges.size(); i++){
					_out_edges.at(i)->set_in_node_done();
				}
				break;
			}
		}
	}
	return NULL;
}

void AugmentationNode::augment_images(std::vector<cv::Mat> imgs, std::vector<int> labels){

	std::vector<cv::Mat> out_imgs;
	std::vector<int> out_labels;
	for(int k=0; k < imgs.size(); k++){
		_counter++;

		for(int i=0; i < _aug_factor; i++){
			_counter++;

			// Define variables
			cv::Mat src(imgs[k]);
			cv::Mat rot_M(2, 3, CV_8U);

			cv::Mat warped_img(src.size(), src.type());
			int label = labels[k];

	  		// construct a trivial random generator engine from a time-based seed:
	  		unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
	  		std::default_random_engine generator (seed);

			// Rotation 
			std::uniform_real_distribution<double> uniform_dist(-1.0, 1.0);
			float theta = 180 * uniform_dist(generator);
			if(theta < -90.0){
				theta = -180.0;
			}
			else if(theta < 0.0){
				theta = -90.0;
			}
			else if(theta < 90.0){
				theta = 0.0;
			}
			else{
				theta = 90.0;
			}

			cv::Point2f warped_img_center(warped_img.cols/2.0F, warped_img.rows/2.0F);
			rot_M = getRotationMatrix2D(warped_img_center, theta, 1.0);
			cv::warpAffine( src, warped_img, rot_M, warped_img.size());

			// Rescaling
			std::uniform_real_distribution<double> sec_uniform_dist(1.0, 1.5);
			int Sx = sec_uniform_dist(generator);
			int Sy = sec_uniform_dist(generator);

			cv::Mat scaling_M(2, 3, CV_32F);
			scaling_M.at<float>(0,0) = Sx;
			scaling_M.at<float>(0,1) = 0;
			scaling_M.at<float>(0,2) = 0;

			scaling_M.at<float>(1,0) = 0;
			scaling_M.at<float>(1,1) = Sy;
			scaling_M.at<float>(1,2) = 0;

			cv::warpAffine( warped_img, warped_img, scaling_M, warped_img.size());

			// Flip image
			std::uniform_real_distribution<double> th_uniform_dist(0.0, 1.0);
			if(th_uniform_dist(generator) > 0.5){
				cv::Mat flipped_img;
				cv::flip(warped_img, flipped_img, 1);
				warped_img = flipped_img;
			}

			// Accumulate
			out_imgs.push_back(warped_img);
			out_labels.push_back(label);
		}

		out_imgs.push_back(imgs[k]);
		out_labels.push_back(labels[k]);
		copy_to_buffer(out_imgs, out_labels);

		// Clean
		out_imgs.clear();
		out_labels.clear();
	}
}