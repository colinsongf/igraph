/*
 *  SCGlib : A C library for the spectral coarse graining of matrices
 *	as described in the paper: Shrinking Matrices while preserving their
 *	eigenpairs with Application to the Spectral Coarse Graining of Graphs.
 *	Preprint available at <http://people.epfl.ch/david.morton>
 *  
 *	Copyright (C) 2008 David Morton de Lachapelle <david.morton@a3.epfl.ch>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 
 *  02110-1301 USA
 *
 *  DESCRIPTION
 *	-----------
 *    This file implements algorithm 5.8 of the above reference.
 *	  The optimal_partition function returns the minimizing partition
 *	  with size 'nt' of the objective function ||v-Pv||, where P is
 *	  a problem-specific projector. So far, Symmetric (matrix=1),
 *	  Laplacian (matrix=2) and Stochastic (matrix=3) projectors
 *	  have been implemented (the cost_matrix function below).
 *	  In the stochastic case, 'p' is expected to be a valid propability
 *	  vector. In all other cases, 'p' is ignored and can be set to NULL.
 *	  The group labels are given in 'gr' as positive consecutive integers
 *	  starting from 0.
 */
 
#include "scg_headers.h"

igraph_real_t igraph_i_scg_optimal_partition(const igraph_vector_t *v, 
					     igraph_vector_long_t *gr,
					     const unsigned int n,
					     const unsigned int nt, 
					     const unsigned int matrix, 
					     const igraph_real_t *p)
{
	/*-----------------------------------------------
	-----Sorts v and counts non-ties-----------------
	-----------------------------------------------*/
	unsigned int i, non_ties;
	igraph_i_scg_indval_t *vs = (igraph_i_scg_indval_t*) igraph_Calloc(n, igraph_i_scg_indval_t);

	igraph_vector_t ps;

	igraph_matrix_t Cv;

	unsigned int q;
	int j;

	igraph_matrix_t F;
	igraph_matrix_long_t Q;
	igraph_real_t temp;
	
	unsigned int l;
	unsigned int part_ind = nt;
	unsigned int col = n-1;
	igraph_real_t sumOfSquares;

	for(i=0; i<n; i++){
	        vs[i].val = VECTOR(*v)[i];
		vs[i].ind = i;
	}

	qsort(vs, n, sizeof(igraph_i_scg_indval_t), igraph_i_scg_compare_ind_val);
	
	non_ties = 1;
	for(i=1; i<n; i++)
		if(vs[i].val != vs[i-1].val) non_ties++;

	if(nt >= non_ties){
		igraph_Free(vs);
		IGRAPH_ERROR("when the optimal method is chosen, values in 'nt' must "
			     "be smaller than the number of unique values in 'v'", 
			     IGRAPH_EINVAL);
	}
	
	//if stochastic SCG orders p
	if(matrix==3){
	  igraph_vector_init(&ps, n);
	  for(i=0; i<n; i++)
	    VECTOR(ps)[i] = p[vs[i].ind];
	}
	/*------------------------------------------------
	------Computes Cv, the matrix of costs------------
	------------------------------------------------*/
	igraph_matrix_init(&Cv, n, n);
	igraph_i_scg_cost_matrix(&Cv, vs, n, matrix, &ps);
	if(matrix==3)
	  igraph_vector_destroy(&ps);
	/*-------------------------------------------------
	-------Fills up matrices F and Q-------------------
	-------------------------------------------------*/					
	/*here j also is a counter but the use of unsigned variables
	is to be proscribed in "for(unsigned int j=...;j>=0;j--)",
	for such loops never ends!*/
	igraph_matrix_init(&F, n, nt);
	igraph_matrix_long_init(&Q, n, nt);
						
	for(i=0; i<n; i++) MATRIX(Q, i, 0)++;
	for(i=0; i<nt; i++) MATRIX(Q, i, i)=i+1;
	
	for(i=0; i<n; i++)
	  MATRIX(F,i,0) = MATRIX(Cv,0,i);
		
	for(i=1; i<nt; i++)
		for(j=i+1; j<n; j++){
		  MATRIX(F, j, i) = MATRIX(F,i-1,i-1) + MATRIX(Cv,i,j);
			MATRIX(Q,j,i) = 2;
		
			for(q=i-1; q<=j-1; q++){
			        temp = MATRIX(F,q,i-1) + MATRIX(Cv,q+1,j);
				if(temp<MATRIX(F,j,i)){
				        MATRIX(F,j,i) = temp;
					MATRIX(Q,j,i) = q+2;
				}
			}
		}
	igraph_matrix_destroy(&Cv);
	/*--------------------------------------------------
	-------Back-tracks through Q to work out the groups-
	--------------------------------------------------*/

	for(j=nt-1; j>=0; j--){
	  for(i=MATRIX(Q,col,j)-1; i<=col; i++)
	    VECTOR(*gr)[vs[i].ind] = part_ind-1 + FIRST_GROUP_NB;
	  if(MATRIX(Q,col,j) != 2){
	    col = MATRIX(Q,col,j)-2;
	    part_ind -= 1;
	  } else{
	    if(j>1){
	      for(l=0; l<=(j-1); l++)
		VECTOR(*gr)[vs[l].ind] = l + FIRST_GROUP_NB;
	      break;
	    } else{
	      col = MATRIX(Q,col,j)-2;
	      part_ind -= 1;
	    }
	  }
	}
	
	sumOfSquares = MATRIX(F,n-1,nt-1);

	igraph_matrix_long_destroy(&Q);
	igraph_matrix_destroy(&F);
	igraph_Free(vs);

	return sumOfSquares;
}

void igraph_i_scg_cost_matrix(igraph_matrix_t *Cv, const igraph_i_scg_indval_t *vs, const unsigned int n, const unsigned int matrix, const igraph_vector_t *ps)
{
	//if symmetric of Laplacian SCG -> same Cv
	if(matrix==1 || matrix==2){
		unsigned int i,j;
		igraph_vector_t w, w2;
		igraph_vector_init(&w, n+1);
		igraph_vector_init(&w2, n+1);
	
		VECTOR(w)[1] = vs[0].val;
		VECTOR(w2)[1] = vs[0].val*vs[0].val;
	
		for(i=2; i<=n; i++){
		  VECTOR(w)[i] = VECTOR(w)[i-1] + vs[i-1].val;
		  VECTOR(w2)[i] = VECTOR(w2)[i-1] + vs[i-1].val*vs[i-1].val;
		}
	
		for(i=0; i<n; i++)
		  for(j=i+1; j<n; j++) {
		    igraph_real_t val=(VECTOR(w2)[j+1]-VECTOR(w2)[i])-(VECTOR(w)[j+1]-VECTOR(w)[i])*(VECTOR(w)[j+1]-VECTOR(w)[i])/(j-i+1);
		    MATRIX(*Cv,i,j)=MATRIX(*Cv,j,i)=val;
		  }
					    
		igraph_vector_destroy(&w);
		igraph_vector_destroy(&w2);
	}
	//if stochastic
	//TODO: optimize it to O(n^2) instead of O(n^3) (as above)
	if(matrix==3){
		unsigned int i,j,k;
		igraph_real_t t1,t2;
		for(i=0; i<n; i++){
			for(j=i+1; j<n; j++){
				t1 = t2 = 0;
				for(k=i; k<j; k++){
				        t1 += VECTOR(*ps)[k];
					t2 += VECTOR(*ps)[k]*vs[k].val;
				}
				t1 = t2/t1;
				t2 = 0;
				for(k=i; k<j; k++)
				  t2 += (vs[k].val-t1)*(vs[k].val-t1);
				MATRIX(*Cv,i,j)=MATRIX(*Cv,j,i)=t2;
			}
		}
	}
	
}

