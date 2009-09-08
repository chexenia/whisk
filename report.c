#include "compat.h"
#include "traj.h"
#include "utilities.h"
#include "common.h"
#include <stdio.h>
#include <string.h>

#define DEBUG_REPORT_1

// The number of bins was set by taking the minimum power of two such that
// comparison of two identical files resulted in no differences.
//
// Setting this too low results in cases where two choices occasionally
// look equally probable when only one is correct.
//
// For the task here, want to have way too many bins.  Treat distributions
// more like a hash table (?).  So only equal whiskers fall into same bin.
// 
#define COMPARE_IDENTITIES_DISTS_NBINS 8096

// Planning:
// 1. Assume measurements files are from same movie
//    
//    Whisker shapes should be nearly the same for corresponding whiskers,
//    but not exactly the same.
//
//    Do this because we want to allow comparison with curated data.
//
// 2. Catalogue types - per segment
//
//    Misidentification
//    Segment missing
//    (no error)
//
// 3. Framewise stats
//
//    histogram # of differences/frame
//    max misidentification streak
//    max missing           streak
//    max ok                streak
//

Measurements* find_match( Distributions *query_change_dist, 
                          Measurements *query, 
                          Distributions *domain_change_dist, 
                          Measurements *domain, 
                          int ndomain, 
                          int minstate,
                          double thresh )
{ double max = -DBL_MAX;
  int argmax = -1;
  int i;
  
  // Find most probable match
  for(i=0; i<ndomain; i++)
  { Measurements *c = domain + i;
    double logp = Eval_Velocity_Likelihood_Log2( domain_change_dist, 
                                                 query->data, 
                                                 c->data, 
                                                 c->state - minstate )
                + Eval_Velocity_Likelihood_Log2( query_change_dist, 
                                                 c->data, 
                                                 query->data, 
                                                 query->state - minstate );
    if( logp > max )
    { max = logp;
      argmax = i;
    }
  }

  // check match against threshold 
  if( max > thresh )
  { return domain + argmax;
  } else {
    return NULL;
  }
}

void print_hist( int *hist, int n)
{ int ttl = 0;
  while(n--)
  { ttl += hist[n];
    printf("%5d: %8d\n", n, hist[n]);
  }
  printf("Total: %8d\n",ttl);
}

#ifdef TEST_REPORT_COMPARE_TRAJECTORIES

char* Spec[] = {"<measurements1:string> <measurements2:string>",NULL};
int main(int argc, char* argv[])
{ Measurements *A, *B;
  int nA, nB, nAst, nBst;
  Measurements *rowA, *rowB, *markA, *markB;
  Distributions *distA, *distB;
  int *hist; 
  int *counts;
  size_t hist_size = 10;
  int streak_cnt, streak_max, streak_argmax;
  int mismatch_total = 0,
      mismatch_max   = 0;
  int *map;

  hist = Guarded_Malloc( sizeof(int) * hist_size, "alloc hist");
  memset( hist, 0, hist_size );

  Process_Arguments(argc, argv, Spec, 0);
  
  A = Measurements_Table_From_Filename( Get_String_Arg("measurements1"), &nA );
  B = Measurements_Table_From_Filename( Get_String_Arg("measurements2"), &nB );

  //
  // Now build distributions. (Conditioned on whisker or not)
  //
  
  Sort_Measurements_Table_State_Time(A, nA);
  distA = Build_Velocity_Distributions( A, nA, COMPARE_IDENTITIES_DISTS_NBINS );
  Distributions_Normalize( distA );
  Distributions_Apply_Log2( distA );
  nAst = _count_n_states( A, nA, 0, NULL, NULL);
  
  Sort_Measurements_Table_State_Time(B, nB);
  distB = Build_Velocity_Distributions( B, nB, COMPARE_IDENTITIES_DISTS_NBINS );
  Distributions_Normalize( distB );
  Distributions_Apply_Log2( distB );
  nBst = _count_n_states( B, nB, 0, NULL, NULL);
 
#ifdef  DEBUG_REPORT_1
  debug("nAst: %d\n"
        "nBst: %d\n", nAst, nBst );
#endif
  //
  // Check for differences in identification
  // =======================================
  
  Sort_Measurements_Table_Time_State_Face( A, nA );
  Sort_Measurements_Table_Time_State_Face( B, nB );
  
  counts = Guarded_Malloc( nAst*nBst*sizeof(int), "alloc counts");
  memset( counts, 0, nAst*nBst*sizeof(int) );
  map = Guarded_Malloc( nAst*sizeof(int), "alloc counts");

  //
  // First pass
  // solve correspondence between trajectories
  //
  rowA = A;
  rowB = B;
  while( rowA < A + nA )
  { 
    int cur = rowA->fid;
    int nframeB;
    int mismatch = 0;
  
    markA = rowA;
    markB = rowB;
  
    // count number in current frame in B
    while( (rowB < B+nB) && (rowB->fid == cur ) )
      rowB++;
    nframeB = rowB - markB;
  
    // iterate over current frame in A
    // Compute # of mismatches
    for(;  ( rowA < A + nA ) && (rowA->fid == cur); rowA++ )
    { Measurements *match;
  
      if( rowA->state == -1 ) // skip these
        continue;
  
      match = find_match( distA, rowA, distB, markB, nframeB, -1, -5000.0 );
      if(match)                                       //assumes minstate == -1
      { counts[ nAst*(match->state+1) + (rowA->state+1) ]++;
      } else {
        counts[ (rowA->state+1) ]++;
      }
    }
  
  }

#ifdef  DEBUG_REPORT_1
  { //print the counts matrix
    int i,j;
    int *c = counts;
    debug("Identity correspondance matrix:\n");
    for(j=0; j<nBst; j++)
    { for(i=0; i<nAst; i++)
        debug("%5d ",*c++);
      debug("\n");
    }
  }
#endif

  //
  // Build mapping from A identities to B identities
  // Greedily
  //
  { int i,j,max;
    for(i=0;i<nAst;i++)
    { max = 0;
      for(j=0;j<nBst;j++)
      { int v = counts[nAst*j + i];
        if( v > max )
        { max = v;
          map[i] = j; // want states, not index  (so subtract 1)
        }
      }
    }
  }

#ifdef  DEBUG_REPORT_1
  { // print mapping of identities in A to B
    int i;
    debug("\n"
          "Identity correspondance\n"
          "  A      B\n"
          " ---    ---\n");
    for( i=0; i<nAst; i++ )
      debug("%3d  ->%3d\n", i-1, map[i]-1);
  }
#endif


  //
  // Second pass
  // Histogram mismatches per frame
  //
  rowA = A;
  rowB = B;
  while( rowA < A + nA )
  { 
    int cur = rowA->fid;
    int nframeB;
    int mismatch = 0;
  
    markA = rowA;
    markB = rowB;
  
    // count number in current frame in B
    while( (rowB < B+nB) && (rowB->fid == cur ) )
      rowB++;
    nframeB = rowB - markB;
  
    // iterate over current frame in A
    // Compute # of mismatches
    mismatch = 0;
    for(;  ( rowA < A + nA ) && (rowA->fid == cur); rowA++ )
    { Measurements *match;
    
      if( rowA->state == -1 ) // skip these
        continue;
    
      match = find_match( distA, rowA, distB, markB, nframeB, -1, -5000.0 );
      if( match && ( map[ rowA->state ] != match->state ) ) 
      { mismatch++;
#ifdef DEBUG_REPORT_1
        if(match->state != -1 )
          debug("Frame %5d. Mismatch\tident:(%3d, %-3d) wid:(%3d, %-3d)\n", cur, 
              rowA->state, 
              match->state,
              rowA->wid,
              match->wid);
#endif
      }
    }
    
    hist = request_storage( hist, &hist_size, sizeof(int), mismatch+1, "request bin for hist" );
    hist[mismatch]++;
    mismatch_total += mismatch;
    mismatch_max = MAX( mismatch_max, mismatch );
    mismatch = 0;
    
    rowA++;
  }
  printf("\nHistogram of # differences per frame.\n");
  print_hist(hist,mismatch_max+1);
  free(counts);
  return 0;
}

#endif
