#include "stencil.h"


int main(int argc, char **argv) {
	if (4 != argc) {
		printf("Usage: stencil input_file output_file number_of_applications\n");
		return 1;
	}
	char *input_name = argv[1];
	char *output_name = argv[2];
	int num_steps = atoi(argv[3]);

	int size;
	int rank;
	MPI_Init(&argc, &argv);               /* Initialize MPI               */
    MPI_Comm_size(MPI_COMM_WORLD, &size); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */


    int num_values;
    double *whole_input;
    if(rank == 0){ // read data and send it too other
        if (0 > (num_values = read_input(input_name, &whole_input))) {
            return 2;
        }
    }
    //send num_vals first
    MPI_Bcast(&num_values, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int ind_num_values = num_values/size; // number of values each processer gets. (size | num_values) assumed!
    double *input;
    if (NULL == (input = malloc(ind_num_values * sizeof(double)))) {
	    printf("We are in process %d\n",rank);
		perror("Couldn't allocate memory for individual input");
		return 2;
    }
    MPI_Scatter(whole_input, ind_num_values, MPI_DOUBLE, input, ind_num_values, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    if(rank == 0){
        free(whole_input);
    }

	// Stencil values
	double h = 2.0*PI/num_values;
	const int STENCIL_WIDTH = 5;
	const int EXTENT = STENCIL_WIDTH/2;
	const double STENCIL[] = {1.0/(12*h), -8.0/(12*h), 0.0, 8.0/(12*h), -1.0/(12*h)};
	// Start timer
	MPI_Barrier(MPI_COMM_WORLD);
	double start = MPI_Wtime();
	// Allocate data for result
	double *output;
	if (NULL == (output = malloc(ind_num_values * sizeof(double)))) {
	    printf("We are in process %d\n",rank);
		perror("Couldn't allocate memory for individual output");
		return 2;
	}

	//Parallel version of repeatedly apply stencil
	for (int s=0; s<num_steps; s++) {
		// Apply stencil
		double l_sendbuf[] = {input[0], input[1]};
		double r_sendbuf[] = {input[ind_num_values - 2], input[ind_num_values-1]};
		double l_recvbuf[2];
		double r_recvbuf[2];
		//We Need to send values here
		MPI_Status status;
		MPI_Request request[2];
		MPI_Isend(l_sendbuf, 2, MPI_DOUBLE, (rank-1+size)%size, 0, MPI_COMM_WORLD, &request[0]); // koppling 0
		MPI_Isend(r_sendbuf, 2, MPI_DOUBLE, (rank+1)%size, 1, MPI_COMM_WORLD, &request[1]); // koppling 1
        MPI_Recv(r_recvbuf, 2, MPI_DOUBLE, (rank+1)%size, 0, MPI_COMM_WORLD, &status); // koppling 0
        MPI_Recv(l_recvbuf, 2, MPI_DOUBLE, (rank-1+size)%size, 1, MPI_COMM_WORLD, &status); // koppling 1
//        MPI_Waitall(2, request, MPI_STATUSES_IGNORE);

		// the two values which need to get values to the left
		for (int i=0; i<EXTENT; i++) {
			double result = 0;
			for (int j=0; j<STENCIL_WIDTH; j++) {
			    if(j<EXTENT-i){

					result += STENCIL[j] * l_recvbuf[j+i];
				}
				else{
				    int index = (i - EXTENT + j);
					result += STENCIL[j] * input[index];
				}
			}
			output[i] = result;
		}
		//For the bulk
		for (int i=EXTENT; i<ind_num_values-EXTENT; i++) {
			double result = 0;
			for (int j=0; j<STENCIL_WIDTH; j++) {
				int index = i - EXTENT + j;
				result += STENCIL[j] * input[index];
			}
			output[i] = result;
		}
		// the two values which need to get values to the right
		for (int i=ind_num_values-EXTENT; i<ind_num_values; i++) {
			double result = 0;
			for (int j=0; j<STENCIL_WIDTH; j++) {
			    if(j< EXTENT + ind_num_values - i){
					int index = (i - EXTENT + j);
	                result += STENCIL[j] * input[index];
                }
                else{
                    result += STENCIL[j] * r_recvbuf[j+EXTENT-(ind_num_values-1-i)-STENCIL_WIDTH];
                }
			}
			output[i] = result;
		}
		// Swap input and output
		if (s < num_steps-1) {
			double *tmp = input;
			input = output;
			output = tmp;
		}
	}
	free(input);

	// Stop timer and get max result
	double my_execution_time = MPI_Wtime() - start;
	double global_time;
	MPI_Reduce(&my_execution_time,&global_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);


	// join results
	double *whole_output;
	if(rank == 0){
		if (NULL == (whole_output = malloc(num_values * sizeof(double)))) {
			perror("Couldn't allocate memory for whole output");
		return 2;
		}
	}
	MPI_Gather(output, ind_num_values, MPI_DOUBLE, whole_output, ind_num_values, MPI_DOUBLE, 0, MPI_COMM_WORLD);
	// Unassign here
	free(output);
	// Write result
	if(rank==0){
	    printf("%f\n", global_time);
	}

#ifdef PRODUCE_OUTPUT_FILE
    // Only Process 0 should write output
    if(rank == 0){
        if (0 != write_output(output_name, whole_output, num_values)) {
		    return 2;
	    }
    }
#endif
	// Clean up
	if(rank == 0){
	    free(whole_output);
	}
	MPI_Finalize();  /* End MPI */
	return 0;
}


int read_input(const char *file_name, double **values) {
	FILE *file;
	if (NULL == (file = fopen(file_name, "r"))) {
		perror("Couldn't open input file");
		return -1;
	}
	int num_values;
	if (EOF == fscanf(file, "%d", &num_values)) {
		perror("Couldn't read element count from input file");
		return -1;
	}
	if (NULL == (*values = malloc(num_values * sizeof(double)))) {
		perror("Couldn't allocate memory for input");
		return -1;
	}
	for (int i=0; i<num_values; i++) {
		if (EOF == fscanf(file, "%lf", &((*values)[i]))) {
			perror("Couldn't read elements from input file");
			return -1;
		}
	}
	if (0 != fclose(file)){
		perror("Warning: couldn't close input file");
	}
	return num_values;
}


int write_output(char *file_name, const double *output, int num_values) {
	FILE *file;
	if (NULL == (file = fopen(file_name, "w"))) {
		perror("Couldn't open output file");
		return -1;
	}
	for (int i = 0; i < num_values; i++) {
		if (0 > fprintf(file, "%.4f ", output[i])) {
			perror("Couldn't write to output file");
		}
	}
	if (0 > fprintf(file, "\n")) {
		perror("Couldn't write to output file");
	}
	if (0 != fclose(file)) {
		perror("Warning: couldn't close output file");
	}
	return 0;
}
