
#include <iostream>
#include <math.h>
#include<mpi.h>
#include<stdio.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header
#pragma once

#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>
using namespace std;
using namespace msclr::interop;


int* inputImage(int* w, int* h, System::String^ imagePath) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;
	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];
	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}


void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);

	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "result Image Saved " << index << endl;
}

// Median Fliter
int median(int* arr, int windowsize) {
	// size of mask
	int MaskSq = windowsize * windowsize;

	// Selection Sort
	int i, j, min, temp;
	for (i = 0; i < MaskSq - 1; i++) {
		min = i;
		for (j = i + 1; j < MaskSq; j++)
			if (arr[j] < arr[min])
				min = j;
		temp = arr[i];
		arr[i] = arr[min];
		arr[min] = temp;
	}

	// select middle element
	return arr[(MaskSq) / 2];
}

// fill mask with numbers
int median_process(int* ImageData, int windowSize, int ImageWidth, int shifft_W, int shifft_H) {
	// create Mask
	int* window = new int[windowSize * windowSize];
	int index = 0;
	for (int i = 0; i < windowSize; i++) {
		for (int j = 0; j < windowSize; j++) {
			// fill mask using shifft width & shifft height
			// if window size is 3 :
			// j + (i * ImageWidth) + shifft_W => which 3 element will be select from row
			//(ImageWidth * shifft_H) => which Row will be select

			window[index] = ImageData[j + (i * ImageWidth) + (shifft_W)+(ImageWidth * shifft_H)];
			index++;
		}
	}

	return median(window, windowSize);

}




int main()
{

	int ImageWidth = 4, ImageHeight = 4;

	int start_s, stop_s, TotalTime = 0;

	System::String^ imagePath;
	std::string img;

	//img = "..//Data//Input//N_N_Salt_Pepper.png";
	//img = "..//Data//Input//2N_N_Salt_Pepper.png";
	//img = "..//Data//Input//5N_N_Salt_Pepper.png";
	//img = "..//Data//Input//10N_N_Salt_Pepper.png";
	img = "..//Data//Input//test2.png";

	imagePath = marshal_as<System::String^>(img);
	int* imageData = inputImage(&ImageWidth, &ImageHeight, imagePath);
	int full_size = ImageHeight * ImageWidth;


	int rank, size;

	int check = 1;	
	int* new_image = new int[full_size];

	MPI_Init(NULL, NULL);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	int windowSize;

	// get window size
	if (rank == 0) {

		cout << "Enter window size: ";
		cin >> windowSize;
	}
	// Broadcast the window size to all process
	MPI_Bcast(&windowSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Barrier(MPI_COMM_WORLD);

	MPI_Status stat;

	// Each rank will create for loop for example if size of process is 10 so rank = 0 will iterate m = 0,10,20,30 and rank = 1 will iterate m = 1,11,21,31 ...
	//if (rank != size - 1) {
	int new_size =  (ImageHeight-(windowSize-1))*(ImageWidth- (windowSize - 1));
	int partition = new_size / (size - 1);
	int* Median_Array = new int[partition];
	int* result = new int[full_size];

	MPI_Bcast(Median_Array, partition, MPI_INT, 0, MPI_COMM_WORLD);
	int Counter = 0;
	

	for (int m = rank; m < full_size; m = m + size - 1) {

		// calculate shifft width using m to pass this shifft to function median_process
		int shifft_W = m % ImageWidth;

		// calculate shifft height using m to pass this shifft to function median_process
		int shifft_H = m / ImageWidth;

		// if image width is less than shifft width + windowSize so we exceed the image width so we will ignore last 2 columns  
		if (ImageWidth <= shifft_W + windowSize) {
			shifft_W = 0;
			shifft_H++;
		}

		// if image height is less than shifft height + windowSize so we exceed the image height so we will ignore last 2 Rows and we reached to end of image
		if (ImageHeight < shifft_H + windowSize) {
			break;
		}

		// all process will calculate median filter except last process & send it to last process
		// last process will store all median filter valus in new_image array
		if (rank != size - 1) {
			// all process will enter except last process

			int median_value = median_process(imageData, windowSize, ImageWidth, shifft_W, shifft_H);
			//int Counter = 0;
			Median_Array[Counter] = median_value;
			//cout << "rank = " << rank << "index = " << Counter << "value = " << Median_Array[Counter] << endl;
		}
		Counter++;
		if (Counter > partition)
			break;
		MPI_Barrier(MPI_COMM_WORLD);
		

	}
	
	MPI_Barrier(MPI_COMM_WORLD);

	if (rank != (size - 1)) {
		cout << "sending....  " << rank << endl;
		MPI_Send(Median_Array, partition, MPI_INT, size - 1, 0, MPI_COMM_WORLD);
	}
	
	if (rank == (size - 1)) {

		for (int i = 0; i < size - 1; i++) {
			cout << "receiving....  " << i << endl;
			MPI_Recv(&result[i* partition], partition, MPI_INT, i, 0, MPI_COMM_WORLD, &stat);
		}

		int* final_result = new int[full_size];
		int index = 0;
		
		for (int i = 0; i < partition; i++) {
			for (int j = 0; j < (size - 1); j++) {
				final_result[index] = result[(j*partition) + i];
				//cout << "index of final result = " << index << " index of result = " << (j * partition) + i << " value = " << final_result[index] << " true value = " << result[(j * partition) + i] << endl;
				index++;
			}
		}
		start_s = clock();
		createImage(final_result, ImageWidth, ImageHeight, 5);
		stop_s = clock();
		TotalTime += (stop_s - start_s) / double(CLOCKS_PER_SEC) * 1000;
		cout << "time: " << TotalTime << endl;
		delete[] new_image;
		delete[] final_result;
		free(imageData);
	}

	// end
	MPI_Finalize();


	return 0;

}