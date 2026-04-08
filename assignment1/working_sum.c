#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

	for(i=0; i<num_steps;i++){
        sum += numbers[i];
    }
	printf("Sum without MPI is: %d\n", sum);
	sum = 0;

	int num_proc;
	int rank;
	MPI_Status status;
	MPI_Init(&argc, &argv);               /* Initialize MPI               */
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */
    printf("we have %d processes\n",num_proc);

    int ssum[num_proc];
    int hole = num_steps/num_proc;
    int rest = num_steps%num_proc;

    // calculate sum in each processor
    // reducera
    ssum[rank] = compute_sum(rank, num_proc, hole, rest, numbers);
    int rec[num_proc];

    printf("We have summed the array and process %d have gotten %d\n", rank, ssum[rank]);

    // reduce
    for(i=1; i < num_proc; i = i*2){
        printf("DEBUG for loop rank %d\n",rank);
        int test = (rank%(i*2));
        printf("DEBUG rest is %d\n",test);
        rec[rank]=0;
        if(rank%(i*2) != 0)
        {
            printf("DEBUG true if statement rank %d\n",rank);
            MPI_Send(&ssum[rank], 1, MPI_INT, (rank-(i)), 111, MPI_COMM_WORLD);
        }
        else
        {
            printf("DEBUG else if statement rank %d\n",rank);
            MPI_Recv(&rec[rank], 1, MPI_INT, (rank + (i)), 111, MPI_COMM_WORLD, &status);
        }
        int rec_val = rec[rank];
        printf("DEBUG: rec_val = %d\n",rec_val);
        ssum[rank] = rec_val + ssum[rank];
    }
    printf("ssum after is %d\n",ssum[rank]);
    printf("ssum[0] is %d for rank %d\n", ssum[0], rank);


    if(rank == 0){
        sum = ssum[0];
        printf("sum is %d\n",sum);
    }
    MPI_Finalize();
	printf("Parallel Summation\n");

	return 0;
}
