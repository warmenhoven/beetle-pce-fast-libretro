/*  dvdisaster: Additional error correction for optical media.
 *  Copyright (C) 2004-2007 Carsten Gnoerlich.
 *  Project home page: http://www.dvdisaster.com
 *  Email: carsten@dvdisaster.com  -or-  cgnoerlich@fsfe.org
 *
 *  The Reed-Solomon error correction draws a lot of inspiration - and even code -
 *  from Phil Karn's excellent Reed-Solomon library: http://www.ka9q.net/code/fec/
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA,
 *  or direct your browser at http://www.gnu.org.
 */

#include "dvdisaster.h"

#include "galois-inlines.h"

/***
 *** Galois field arithmetic.
 *** 
 * Calculations are done over the extension field GF(2**n).
 * Be careful not to overgeneralize these arithmetics;
 * they only work for the case of GF(p**n) with p being prime.
 */

/* Initialize the Galois field tables */


GaloisTables* CreateGaloisTables(int32_t gf_generator)
{  
   GaloisTables *gt = (GaloisTables *)calloc(1, sizeof(GaloisTables));
   int32_t b,log;

   /* Allocate the tables.
      The encoder uses a special version of alpha_to which has the mod_fieldmax()
      folded into the table. */

   gt->gfGenerator = gf_generator;

   gt->indexOf     = (int32_t *)calloc(GF_FIELDSIZE, sizeof(int32_t));
   gt->alphaTo     = (int32_t *)calloc(GF_FIELDSIZE, sizeof(int32_t));
   gt->encAlphaTo  = (int32_t *)calloc(2*GF_FIELDSIZE, sizeof(int32_t));
   
   /* create the log/ilog values */

   for(b=1, log=0; log<GF_FIELDMAX; log++)
   {  gt->indexOf[b]   = log;
      gt->alphaTo[log] = b;
      b = b << 1;
      if(b & GF_FIELDSIZE)
	b = b ^ gf_generator;
   }

   /* we're even closed using infinity (makes things easier) */

   gt->indexOf[0] = GF_ALPHA0;    /* log(0) = inf */
   gt->alphaTo[GF_ALPHA0] = 0;   /* and the other way around */

   for(b=0; b<2*GF_FIELDSIZE; b++)
     gt->encAlphaTo[b] = gt->alphaTo[mod_fieldmax(b)];

   return gt;
}

void FreeGaloisTables(GaloisTables *gt)
{
  if(gt->indexOf)     free(gt->indexOf);
  if(gt->alphaTo)     free(gt->alphaTo);
  if(gt->encAlphaTo) free(gt->encAlphaTo);

  free(gt);
}

/***
 *** Create the the Reed-Solomon generator polynomial
 *** and some auxiliary data structures.
 */

ReedSolomonTables *CreateReedSolomonTables(GaloisTables *gt,
					   int32_t first_consecutive_root,
					   int32_t prim_elem,
					   int nroots_in)
{  ReedSolomonTables *rt = (ReedSolomonTables *)calloc(1, sizeof(ReedSolomonTables));
   int32_t i,j,root;

   rt->gfTables = gt;
   rt->fcr      = first_consecutive_root;
   rt->primElem = prim_elem;
   rt->nroots   = nroots_in;
   rt->ndata    = GF_FIELDMAX - rt->nroots;

   rt->gpoly    = (int32_t *)calloc((rt->nroots+1), sizeof(int32_t));

   /* Create the RS code generator polynomial */

   rt->gpoly[0] = 1;

   for(i=0, root=first_consecutive_root*prim_elem; i<rt->nroots; i++, root+=prim_elem)
   {  rt->gpoly[i+1] = 1;

     /* Multiply gpoly  by  alpha**(root+x) */

     for(j=i; j>0; j--)
     {
       if(rt->gpoly[j] != 0)
         rt->gpoly[j] = rt->gpoly[j-1] ^ gt->alphaTo[mod_fieldmax(gt->indexOf[rt->gpoly[j]] + root)]; 
       else
	 rt->gpoly[j] = rt->gpoly[j-1];
     }

     rt->gpoly[0] = gt->alphaTo[mod_fieldmax(gt->indexOf[rt->gpoly[0]] + root)];
   }

   /* Store the polynomials index for faster encoding */ 

   for(i=0; i<=rt->nroots; i++)
     rt->gpoly[i] = gt->indexOf[rt->gpoly[i]];
   return rt;
}

void FreeReedSolomonTables(ReedSolomonTables *rt)
{
  if(rt->gpoly)        free(rt->gpoly);

  free(rt);
}
