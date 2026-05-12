#include "pivot.h"
#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>


// Pivot strategies
#define SMALLEST_ROOT 0
#define MEDIAN_ROOT 1
#define MEAN_MEDIAN 2
#define MEDIAN_MEDIAN 3

// Root node
#define ROOT 0

/**
 * @param v1 Pointer to first value (integer) to compare
 * @param v2 Pointer to second value (integer) to compare
 * @return 0 if *v1==*v2, a positive and negative number of *v1>*v2 and *v1<*v2 respectively
 */
int compare(const void *v1, const void *v2){
    if(v1 == NULL || v2 == NULL){
        perror("Wrong input in compare");
        return -1;
    }
    int v1_val = *(int*)v1;
    int v2_val = *(int*)v2;
    if(v1_val == v2_val){
        return 0;
    }
    return (v1_val > v2_val) ? 1 : -1;
}

/**
 * Find the index of the first value in elements that is larger than val
 * @param elements Array to search in
 * @param n Length of elements
 * @param val Value to search for
 * @return index of first value that is larger than val, or length of array if all values are smaller
 */
int get_larger_index(int *elements, int n, int val){
    if(n < 0){
        perror("Index out of range");
        return -1;
    }
    if(n==0){
        return 0;
    }
    if(elements==NULL){
        perror("Element in list is NULL");
        return -1;
    }

    if(elements[n-1] <= val){
        // printf("elements[n-1] = %d\n",elements[n-1]);
        return n;
    }
    else if(elements[0] > val){
        // printf("elements[0] = %d\n",elements[0]);
        return 0;
    }
    else{
        int i = n-1;
        while(elements[i] > val){
            i = i >> 1;
        }
        for(; i <n; i++){
            if(elements[i]> val){
                return i;
            }
        }
    }
    // else{
    //     for(int i = 1; i < n; i++){
    //         if(elements[i]> val){
    //             return i;
    //         }
    //     }
    // }
    return -2;
}

/**
 * Find the median in an array. Note that this function assumes that the array
 * is sorted!
 * @param elements Sorted array
 * @param n Length of elements
 * @return median of elements
 */
int get_median(int *elements, int n){
    if(elements == NULL){
        perror("Element is null");
        return -2;
    }
    if(n < 0){
        perror("Index out of range");
        return -1;
    }
    if(n==0){
        return 0;
    }
    int i = n >> 1;
    return elements[i];
}

/**
 * Select a pivot element for parallel quick sort. Return the index of the first
 * element that is larger than the pivot. Note that this function assumes that
 * elements is sorted!
 * @param pivot_strategy 0=>smallest on root (Not recommended!) 1=>median on root 2=>mean of medians 3=>median of medians
 * @param elements Elements stored by the current process (sorted!)
 * @param n Length of elements
 * @param communicator Communicator for processes in current group
 * @return The index of the first element after pivot
 */
int select_pivot(int pivot_strategy, int *elements, int n, MPI_Comm communicator){
    if(pivot_strategy == SMALLEST_ROOT){
        return select_pivot_smallest_root(elements, n, communicator);
    }
    else if(pivot_strategy == MEDIAN_ROOT){
        return select_pivot_median_root(elements, n, communicator);
    }
    else if(pivot_strategy == MEAN_MEDIAN){
        return select_pivot_mean_median(elements, n, communicator);
    }
    else if(pivot_strategy == MEDIAN_MEDIAN){
        return select_pivot_median_median(elements, n, communicator);
    }
    else{
        perror("Pivot strategy outside of index");
        return -2;
    }
}

/**
 * See select pivot!
 */
int select_pivot_median_root(int *elements, int n, MPI_Comm communicator){
    int rank;
    MPI_Comm_rank(communicator, &rank); /* Get my number */

    int pivot;
    if(rank == ROOT){
        pivot = get_median(elements, n);
    }
    MPI_Bcast(&pivot, 1, MPI_INT, ROOT, communicator);
    return get_larger_index(elements, n, pivot);
}

/**
 * See select pivot!
 */
int select_pivot_mean_median(int *elements, int n, MPI_Comm communicator){
    int rank;
    int size;
    MPI_Comm_rank(communicator, &rank); /* Get my number */
    MPI_Comm_size(communicator, &size); /* Get the number of processors */

    int ind_mean = get_median(elements, n);
    int *mean_vec = NULL;
    /* Need long int to not go outside bit size */
    long int pivot;
    if(rank == ROOT){
        if (NULL == (mean_vec = malloc(size * sizeof(int)))) {
			perror("Couldn't allocate memory for vector of meadians");
		return -1;
		}
    }
    MPI_Gather(&ind_mean, 1, MPI_INT, mean_vec, 1, MPI_INT, ROOT, communicator);
    if(rank == ROOT){
        pivot = 0;
        for(int j = 0; j < size; j++){
            pivot += mean_vec[j];
        }
        pivot /= size; 
        free(mean_vec);
    }
    MPI_Bcast(&pivot, 1, MPI_INT, ROOT, communicator);
    return get_larger_index(elements, n, pivot);
}

/**
 * See select pivot!
 */
int select_pivot_median_median(int *elements, int n, MPI_Comm communicator){
    int rank;
    int size;
    MPI_Comm_rank(communicator, &rank); /* Get my number */
    MPI_Comm_size(communicator, &size); /* Get the number of processors */

    int ind_mean = get_median(elements, n);
    int *mean_vec = NULL; // to get rid of "warning"
    int pivot;
    if(rank == ROOT){
        if (NULL == (mean_vec = malloc(size * sizeof(int)))) {
			perror("Couldn't allocate memory for vector of meadians");
		return -1;
		}
    }
    MPI_Gather(&ind_mean, 1, MPI_INT, mean_vec, 1, MPI_INT, ROOT, communicator);
    if(rank == ROOT){
        /* sort before finding median */
        qsort(mean_vec, size, sizeof(int), compare);
        pivot = get_median(mean_vec, size);
        free(mean_vec);
    }
    MPI_Bcast(&pivot, 1, MPI_INT, ROOT, communicator);
    return get_larger_index(elements, n, pivot);
}

/**
 * See select pivot!
 */
int select_pivot_smallest_root(int *elements, int n, MPI_Comm communicator){
    int rank;
    MPI_Comm_rank(communicator, &rank); /* Get my number */

    int pivot;
    if(rank == ROOT){
        pivot = (0 < n) ? elements[0] : 0;
    }
    MPI_Bcast(&pivot, 1, MPI_INT, ROOT, communicator);
    return get_larger_index(elements, n, pivot);
}
