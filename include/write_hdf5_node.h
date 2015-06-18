#ifndef _WRITE_HDF5_NODE_H
#define _WRITE_HDF5_NODE_H

#include "node.h"
#include "hdf5.h"
#include "utils.h"
#include <vector>
#include <tuple>
#include <iostream>
#include <cv.h>
#include <opencv2/highgui/highgui.hpp> 
#include <opencv2/core/core.hpp>

class WriteHDF5Node: public Node{

	public:
		WriteHDF5Node(std::string id, std::string fname, std::vector<hsize_t> dim, std::string dataset_name, std::vector<double> labels);
		void *run();
		
  	private:
  		void copy_mat(cv::Mat out);
  		void write_to_disk();
  		void write_labels();
  		std::string _fname;
  		std::vector<hsize_t> _dim;
  		unsigned int _curr_size;
  		unsigned int _h;
  		unsigned int _w;
  		unsigned int _c;
  		int _n_dim;
  		std::vector<double> _file_buffer;
  		int _f_count;
  		int _label_count;
  		std::string _dataset_name;
  		int _el_cont;
  		std::vector<double> _labels;
};	
#endif