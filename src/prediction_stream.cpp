#include "graph_net.h"
#include "laplacian_pyramid_node.h"
#include "read_node.h"
#include "debug_node.h"
#include "write_png_node.h" 
#include "grayscale_node.h"
#include "write_hdf5_node.h"
#include "train_node.h"
#include "prediction_pipe_node.h"
#include "read_pipe_node.h"
#include "edge.h"
#include "hdf5.h"
#include "utils.h"
#include <tuple>
#include <vector>
#include <string>
#include <iostream>
#include <ctime>
#include <boost/thread.hpp>
#include <boost/ptr_container/ptr_deque.hpp>
#include <dirent.h>
#define REPEAT_MODE  0
#define ALTERNATE_MODE  1
#define CHUNK_MODE 2
#define NUMB_GRAYSCALE_NODE 1	
#define NUMB_LAPLACIAN_NODE 1
#define SERIAL 0
#define PARALLEL 1


//const static std::string IMAGE_PATH = "/home/lcoop22/Images/LGG";
//const static std::string LOCAL_HOME = "/home/nnauata";
//const static std::string fname = "/home/nnauata/LGG-test/LGG-Endothelial-2-test.h5";
const static std::string IMAGE_PATH = "/home/nelson/LGG-test";
const static std::string LOCAL_HOME = "/home/nelson";
const static std::string fname = "/home/nelson/LGG-test/LGG-Endothelial-small.h5";

int main (int argc, char * argv[3])
{

	// Store parameters
	int gpu_id = 0;
	int batch_size = 10;
	
	std::cout << "Intializing Prediction" << std::endl; 
	
	//sscanf(argv[1],"%d",&gpu_id);
	//sscanf(argv[2],"%d",&batch_size);

	std::cout << "Running on device " << std::to_string(gpu_id) << std::endl;
	std::cout << "Batch size: " << std::to_string(batch_size) << std::endl;
	
	/********************************    Setup Graphs     *******************************************/
	// Define Graphs
	std::cout << "Defining graph nodes..." << std::endl;
	GraphNet *prediction_graph = new GraphNet(SERIAL);

	// Define prediction nodes
	std::string pipe_name = LOCAL_HOME + "/CellNet/app/pipe"+std::to_string(gpu_id);
	std::string trained_model_path = LOCAL_HOME + "/CellNet/app/cell_net.caffemodel";
	std::string test_model_path = LOCAL_HOME + "/CellNet/online_caffe_model/cnn_test.prototxt";
	std::string model_path = LOCAL_HOME + "/CellNet/online_caffe_model/cnn_train_val.prototxt";
	prediction_graph->add_node(new PredictionPipeNode("prediction_node", REPEAT_MODE, batch_size, test_model_path, trained_model_path, gpu_id, pipe_name));
	
	std::cout << "Defining Edges" << std::endl;
	// Add edges
	int n_edges = 0;
	prediction_graph->add_edge(new Edge("edge" + std::to_string(n_edges++), "read_pipe_node", "prediction_node"));
	
	std::cout << "*Graph defined*" << std::endl;
	
	/********************************************* Run Graphs ***************************************************/
	
	// Run graph
	prediction_graph->run();

	/*********************************************    Clean   ***************************************************/
	
	// Release memory
	delete prediction_graph;

	return 0;
}
