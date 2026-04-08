#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


// small modify
int min(int a, int b) {
    return (a < b) ? a : b;
}

int compute_sum(int rank, int num_proc, int hole, int rest, int *numbers){
    int sssum=0;
    int first_element = rank*hole + min(rest, rank);
    int next_first = (rank+1)*hole + min(rest, (rank+1));
    int j = first_element;
    for(j = first_element; j < next_first ; j++){
        sssum += numbers[j];
    }
    return sssum;
}


int main(int argc, char **argv) {
	if (2 != argc) {
		printf("Usage: sum number_of_summands\n");
		return 1;
	}
	int num_steps = atoi(argv[1]);
	int sum = 0;
	int numbers[num_steps];
	int i;
	for(i=0; i<num_steps; i++){
	    numbers[i] = rand()%1000; // dont want too big values. Now caps at 999
	}

	// for(i=0; i<num_steps;i++){
 //        sum += numbers[i];
 //    }
	// printf("Sum without MPI is: %d\n", sum);
	// sum = 0;

	int num_proc;
	int rank;
	MPI_Status status;
	MPI_Init(&argc, &argv);               /* Initialize MPI               */
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */
    printf("we have %d processes\n",num_proc);

    int ssum;
    int hole = num_steps/num_proc;
    int rest = num_steps%num_proc;

    // calculate sum in each processor and create reciver
    ssum = compute_sum(rank, num_proc, hole, rest, numbers);
    int rec = 0;

    printf("We have summed the array and process %d have gotten %d\n", rank, ssum);

    // reduce
    for(i=1; i < num_proc; i = i*2){
        int test = (rank%(i*2));
        if(rank%(i*2) == i)
        {
            printf("DEBUG we send to rank %d and we are rank %d\n", (rank-(i)) ,rank);
            MPI_Send(&ssum, 1, MPI_INT, (rank-(i)), 111, MPI_COMM_WORLD);
        }
        else if(rank%(i*2) == 0)
        {
            printf("DEBUG we are rank %d and recive from rank %d\n",rank, (rank + (i)));
            MPI_Recv(&rec, 1, MPI_INT, (rank + (i)), 111, MPI_COMM_WORLD, &status);
        }
        int rec_val = rec;
        ssum = rec_val + ssum;
    }

    if(rank == 0){
        sum = ssum;
        printf("sum is %d\n",sum);
    }
    MPI_Finalize();
	// printf("Parallel Summation\n");

	return 0;
}
