#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

//Small change
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
	if (argc != 2) {
		printf("Usage: sum number_of_summands\n");
		return 1;
	}
	int num_steps = atoi(argv[1]);
	int numbers[num_steps];
	for(int i=0; i<num_steps; i++){
	    numbers[i] = rand()%1000; //max int size 999
	}

	int num_proc;
	int rank;
	MPI_Status status;
	MPI_Init(&argc, &argv);               /* Initialize MPI               */
    MPI_Comm_size(MPI_COMM_WORLD, &num_proc); /* Get the number of processors */
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get my number                */

    if(rank == 0){
        int sum = 0;
        for(int i=0; i<num_steps;i++){
            sum += numbers[i];
        }
	    printf("Sum without parallelization is: %d\n", sum);
    }
    

    int ssum;
    ssum = compute_sum(rank, num_proc, num_steps/num_proc, num_steps%num_proc, numbers);
    int rec = 0;

    //printf("We have summed the array and process %d have gotten %d\n", rank, ssum);

    for(int i=1; i < num_proc; i = i*2){
        if(rank%(i*2) == i)
        {
            MPI_Send(&ssum, 1, MPI_INT, rank-i, 111, MPI_COMM_WORLD);
            break; //kill sender
        }
        else if(rank%(i*2) == 0)
        {
            if(rank + i < num_proc)
            {
                MPI_Recv(&rec, 1, MPI_INT, rank + i, 111, MPI_COMM_WORLD, &status);
                int rec_val = rec; //only receiver needs to add.
                ssum = rec_val + ssum;
            }
        }
    }

    if(rank == 0){
        printf("sum is %d\n", ssum);
    }
    MPI_Finalize();

	return 0;
}
