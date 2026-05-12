#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "quicksort.h"
#include "pivot.h"

// if print or not
#define PrintOutput 0

/*
 * added min func
 */
int min(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * Verify that elements are sorted in ascending order. If not, write an error
 * message to stdout. Thereafter, print all elements (in the order they are
 * stored) to a file named according to the last argument.
 * @param elements Elements to check and print
 * @param n Number of elements
 * @param file_name Name of output file
 * @return 0 on success, -2 on I/O error
 */
int check_and_print(int *elements, int n, char *file_name){
    if(!sorted_ascending(elements, n)){
        fprintf(stdout, "Unsorted list\n");
        return -1;
    }
    else{
        if(PrintOutput){
            // Taken from stencil.c and altered
            FILE *file;
            if (NULL == (file = fopen(file_name, "w"))) {
                perror("Couldn't open output file");
                return -2;
            }
            for (int i = 0; i < n; i++) {
                if (0 > fprintf(file, "%d ", elements[i])) {
                    perror("Couldn't write to output file");
                }
            }
            if (0 != fclose(file)) {
    		    perror("Warning: couldn't close output file");
    	    }
        }
    	return 0;
    }
}

/**
 * Distribute all elements from root to the other processes as evenly as
 * possible. Note that this method allocates memory for my_elements. This must
 * be freed by the caller!
 * @param all_elements Elements to distribute (Not significant in other processes)
 * @param n Number of elements in all_elements
 * @param my_elements Pointer to buffer where the local elements will be stored
 * @return Number of elements received by the current process
 */
int distribute_from_root(int *all_elements, int n, int **my_elements){
    int size;
	int rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */

    /* we need to send arrays into MPI_Scatterv(...). Use loop to get length and start pointers*/
    int *sendcounts = malloc(size * sizeof(int));
    int *displs = malloc(size * sizeof(int)); 
    for(int i = 0; i < size; i++){
        int start = (n/size)*i + min(n%size, i);
        int stop = (n/size)*(i+1) + min(n%size, (i+1));
        sendcounts[i] = stop - start;
        displs[i] = start;
    }

    if (NULL == (*my_elements = malloc((sendcounts[rank]) * sizeof(int)))) {
	    printf("We are in process %d\n",rank);
		perror("Couldn't allocate memory for individual elements");
		return -1;
	}
    /* Scatter elements and free memory. Scatterv used to have uneven array length*/
    MPI_Scatterv(all_elements, sendcounts, displs, MPI_INT, *my_elements, sendcounts[rank], MPI_INT, 0, MPI_COMM_WORLD);
    int n_recv =sendcounts[rank];
    free(sendcounts);
    free(displs);
    return n_recv;
}

/**
 * Gather elements from all processes on root. Put root's elements first and
 * thereafter elements from the other nodes in the order of their ranks (so that
 * elements from process i come after the elements from process i-1).
 * @param all_elements Buffer on root where the elements will be stored
 * @param my_elements Elements to be gathered from the current process
 * @param local_n Number of elements in my_elements
 */
void gather_on_root(int *all_elements, int *my_elements, int local_n){
    int size;
	int rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */

    /* Send arrays of count and pointers to MPI_Gatherv(...) */
    int *recvcounts = NULL; // NULL to get rid of "warning"
    int *displs = NULL; // NULL to get rid of "warning"
    if(rank == 0){
        recvcounts = malloc(size * sizeof(int));
        displs = malloc(size * sizeof(int));
    }
    /* First gather the lengths of individual arrays */
    MPI_Gather(&local_n, 1, MPI_INT, recvcounts, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if(rank == 0){
        int constant = 0;
        for(int i = 0; i < size; i++){
            displs[i] = constant;
            constant += recvcounts[i];
        }
    }
    /* Gather arrays dependent on local array length. Gatherv allows  for uneven array lengths */
    MPI_Gatherv(my_elements, local_n, MPI_INT, all_elements, recvcounts, displs, MPI_INT, 0, MPI_COMM_WORLD);
    if(rank == 0){
        free(recvcounts);
        free(displs);
    }
    free(my_elements);
}

/**
 * Perform the global part of parallel quick sort. This function assumes that
 * the elements is sorted within each node. When the function returns, all
 * elements owned by process i are smaller than or equal to all elements owned
 * by process i+1, and the elements are sorted within each node.
 * @param elements Pointer to the array of sorted values on the current node. Will point to a(n) (new) array with the sorted elements when the function returns.
 * @param n Length of *elements
 * @param MPI_Comm Communicator containing all processes participating in the global sort
 * @param pivot_strategy Tells how to select the pivot element. See documentation of select_pivot in pivot.h.
 * @return New length of *elements
 */
int global_sort(int **elements, int n, MPI_Comm communicator, int pivot_strategy){
    int size;
	int rank;
    MPI_Comm_size(communicator, &size); /* Get the number of processors */
    MPI_Comm_rank(communicator, &rank); /* Get my number                */
    /* No need to do anything else. We can end the recurssive call */
    // printf("Rank %d, size %d, with n = %d\n", rank, size,n);
    if(size == 1 || n == 0){
        return n;
    }
    /* Pivot selecting and initializing pointers and values to use later */
    int index = select_pivot(pivot_strategy, *elements, n, communicator);

    int *smaller = *elements; /* pointer at the start */
    int *larger = (*elements) + index; /* pointer ofset by the index*/
    int *recvbuff;
    int *kept;

    int n_smaller = index;
    int n_larger = n-index;
    int n_recv;
    int n_kept;

    int full_n;
    int half_size = size/2; 
    /* Partitioning where ranks are  splti into a lower and higher group. send and recv "parters" are chosen based on the "index" in each group.
    *  ex: 8 processes.  0 <-> 4, 1 <-> 5, 2 <-> 6, 3 <-> 7. Lower group = (0,1,2,3). Higher group = (4,5,6,7).*/
    /* Starting with sending either the number of greater or smaller values based on pivot and "group". Use memcpy to copy relevant values for ease of use */
    MPI_Status status;
    if(rank < half_size ){
        // printf("Rank %d before Sendrecv counts\n", rank);
        // fflush(stdout);
        MPI_Sendrecv(&n_larger, 1, MPI_INT, (rank + half_size), 0,
            &n_recv, 1, MPI_INT, (rank + half_size), 1,
            communicator, &status);
        // printf("Rank %d after Sendrecv counts\n", rank);
        // fflush(stdout);


        kept = malloc(n_smaller * sizeof(int));
        memcpy(kept, smaller, n_smaller * sizeof(int)); // Need if statement
        n_kept = n_smaller; // Need if statement
    }
    else{
        // printf("Rank %d before Sendrecv counts\n", rank);
        // fflush(stdout);
        /*int MPI_Sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                 int dest, int sendtag,
                 void *recvbuf, int recvcount, MPI_Datatype recvtype,
                 int source, int recvtag, MPI_Comm comm, MPI_Status * status)*/
        MPI_Sendrecv(&n_smaller, 1, MPI_INT, (rank - half_size), 1,
            &n_recv, 1, MPI_INT, (rank - half_size), 0,
            communicator, &status);
        // printf("Rank %d after Sendrecv counts\n", rank);
        // fflush(stdout);
        kept = malloc(n_larger * sizeof(int));
        memcpy(kept, larger, n_larger * sizeof(int)); // Need if statement
        n_kept = n_larger; // Need if statement
    }
    /* Allocate for receive buff and send either the larger of lower values to relevant "partner" */

    recvbuff = malloc(n_recv * sizeof(int));
    if(rank < half_size){
        MPI_Sendrecv(larger, n_larger, MPI_INT, (rank + half_size), 10,
            recvbuff, n_recv, MPI_INT, (rank + half_size), 11,
            communicator, &status);
    }
    else{
        MPI_Sendrecv(smaller, n_smaller, MPI_INT, (rank - half_size), 11,
            recvbuff, n_recv, MPI_INT, (rank - half_size), 10,
            communicator, &status);
    }

    // printf("Rank %d, size %d, after all Sendrecv\n", rank, size);
    // fflush(stdout);
    /* Creating result array and merge the relevant sections (smaller or greater) together */
    full_n = n_kept + n_recv;
    int *result = (int *)malloc(full_n * sizeof(int));
    merge_ascending( kept, n_kept, recvbuff, n_recv, result);

    /* Free memory */
    free(kept);
    free(recvbuff);
    free(*elements);
    *elements = result;

    /* Recurssive step. Create new a group and call global_sort recurssively */
    /* 'note from me: can i call a value color?' */
    int color = (rank < half_size)? 0 : 1;
    MPI_Comm MPI_sub_comm;
    MPI_Comm_split(communicator, color, rank, &MPI_sub_comm);
    int new_n = global_sort(elements, full_n, MPI_sub_comm, pivot_strategy);
    MPI_Comm_free(&MPI_sub_comm);

    return new_n;
}

/**
 * Merge v1 and v2 to one array, sorted in ascending order, and store the result
 * in result.
 * @param v1 Array to merge
 * @param n1 Length of v1
 * @param v2 Array to merge
 * @param n2 Length of v2
 * @param result Array for merged result (must be allocated before!)
 */
void merge_ascending(int *v1, int n1, int *v2, int n2, int *result){
    int nn = n1 + n2;
    int i = 0;
    int j = 0;

    /* Have to have 3 checks to ensure we are not outside n1 or n2 */
    for(int k = 0; k < nn; k++){
        if( (j == n2) || (i < n1 && v1[i] < v2[j]) ){
            result[k] = v1[i];
            i++;
        }
        else{
            result[k] = v2[j];
            j++;
        }
    }
}

/**
 * Read problem size and elements from the file whose name is given as an
 * argument to the function and populate elements accordingly. Note that this
 * method allocates memory for elements. This must be freed by the caller!
 * @param file_name Name of input file
 * @param elements Pointer to array to be created and populated
 * @return Number of elements read and stored
 */
int read_input(char *file_name, int **elements){
    /* Taken from stencil.c (assignment 2) */
    /* Just a lot of checks to ensure we read the file correctly */
    FILE *file;
	if (NULL == (file = fopen(file_name, "r"))) {
		perror("Couldn't open input file");
		return -1;
	}
	int n;
	if (EOF == fscanf(file, "%d", &n)) {
		perror("Couldn't read element count from input file");
		return -1;
	}
	if (NULL == (*elements = malloc(n * sizeof(int)))) {
		perror("Couldn't allocate memory for input");
		return -2;
	}
	for (int i=0; i<n; i++) {
		if (EOF == fscanf(file, "%d", &((*elements)[i]))) {
			perror("Couldn't read elements from input file");
			return -2;
		}
	}
	if (0 != fclose(file)){
		perror("Warning: couldn't close input file");
	}
	return n;
}

/**
 * Check if a number of elements are sorted in ascending order. If they aren't,
 * print an error message specifying the first two elements that are in wrong
 * order.
 * @param elements Array to check
 * @param n Length of elements
 * @return 1 if elements is sorted in ascending order, 0 otherwise
 */
int sorted_ascending(int *elements, int n){
    /* No comments needed */
    for(int i = 1; i < n; i++){
        if(elements[i-1] > elements[i]){
            printf("ERROR: Unsorted list! Element[%d] > Element[%d]\n", i-1, i);
            return 0;
        }
    }
    return 1;
}

/**
 * Swap the values pointed at by e1 and e2.
 */
void swap(int *e1, int *e2){
    int tmp = *e2;
    *e2 = *e1;
    *e1 = tmp;
}




int main(int argc, char **argv){
    if (4 != argc) {
		printf("Usage: quicksort input_file output_file pivot_strategy\n");
		return -1;
	}
    char *input_name = argv[1];
	char *output_name = argv[2];
	int pivot_strategy = atoi(argv[3]);

    int size;
	int rank;
    MPI_Init(&argc, &argv);               /* Initialize MPI               */
    MPI_Comm_size(MPI_COMM_WORLD, &size); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */

    MPI_Barrier(MPI_COMM_WORLD);
    double start1 = MPI_Wtime();

    /* Allocate for input and broadcast n to all processes*/
    int *all_elements; 
    int n;
    if(rank == 0){
        if (0 > (n = read_input(input_name, &all_elements))) {
            MPI_Finalize();  /* End MPI before exiting*/
            return -2;
        }
    }
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);

    /* Use "our" function to partition the input array of elements*/
    int *local_elements;
    int local_n = distribute_from_root(all_elements, n, &local_elements);
    if(rank == 0){
        free(all_elements);
    }

    printf("TIME DEBUG. rank %d Time of I/O is %lf\n", rank ,MPI_Wtime()-start1);
    /* Use MPI_Barrier(...) + MPI_Reduce(...) to the the slowest process time */
    
    for(pivot_strategy=1; pivot_strategy < 4; pivot_strategy++){
    printf("pivot_strategy: %d\n", pivot_strategy);
    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    /* Usign qsort() and then calling global_sort() to sort the elements on/into relevant processes*/
    qsort(local_elements, local_n, sizeof(int), compare);
    local_n = global_sort(&local_elements, local_n, MPI_COMM_WORLD, pivot_strategy);

    // Stop timer and get max result
	double my_execution_time = MPI_Wtime() - start;
    double global_time;
	MPI_Reduce(&my_execution_time, &global_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    double start2 = MPI_Wtime();

    if(rank == 0){
        if (NULL == (all_elements = malloc(n * sizeof(int)))) {
			perror("Couldn't allocate memory for output");
		return -2;
		}
    }

    /* Use gather_on_root() to, SURPRISE!, gather all elements on the root processor */
    gather_on_root(all_elements, local_elements, local_n);
    /* Write file, free memory and print runtime*/
    if(rank == 0){
        check_and_print(all_elements, n, output_name);
        free(all_elements);
        printf("pivot strategy: %d. time: %f\n",pivot_strategy, global_time);
        // printf("%f\n", global_time);
        // printf("TIME DEBUG. Time of U/O is %lf\n",MPI_Wtime()-start2);
    }
    }
    MPI_Finalize();  /* End MPI */
    return 0;
}
