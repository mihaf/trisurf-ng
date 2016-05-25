/* vim: set ts=4 sts=4 sw=4 noet : */
#include<stdlib.h>
#include<math.h>
#include "general.h"
#include "vertex.h"
#include "bond.h"
#include "triangle.h"
#include "vesicle.h"
#include "energy.h"
#include "timestep.h"
#include "cell.h"
//#include "io.h"
#include "io.h"
#include<stdio.h>
#include "vertexmove.h"
#include <string.h>
#include "constvol.h"

ts_bool single_verticle_timestep(ts_vesicle *vesicle,ts_vertex *vtx,ts_double *rn){
    ts_uint i;
    ts_double dist;
    ts_bool retval; 
    ts_uint cellidx; 
    ts_double delta_energy, delta_energy_cv,oenergy,dvol=0.0, darea=0.0;
    ts_double costheta,sintheta,phi,r;
	//This will hold all the information of vtx and its neighbours
	ts_vertex backupvtx[20], *constvol_vtx_moved=NULL, *constvol_vtx_backup=NULL;
	memcpy((void *)&backupvtx[0],(void *)vtx,sizeof(ts_vertex));

	//Some stupid tests for debugging cell occupation!
/*     	cellidx=vertex_self_avoidance(vesicle, vtx);
	if(vesicle->clist->cell[cellidx]==vtx->cell){
		fprintf(stderr,"Idx match!\n");
	} else {
		fprintf(stderr,"***** Idx don't match!\n");
		fatal("ENding.",1);
	}
*/

    	//temporarly moving the vertex
//	vtx->x=vtx->x+vesicle->stepsize*(2.0*rn[0]-1.0);
//    	vtx->y=vtx->y+vesicle->stepsize*(2.0*rn[1]-1.0);
//    	vtx->z=vtx->z+vesicle->stepsize*(2.0*rn[2]-1.0);

//random move in a sphere with radius stepsize:
	r=vesicle->stepsize*rn[0];
	phi=rn[1]*2*M_PI;
	costheta=2*rn[2]-1;
	sintheta=sqrt(1-pow(costheta,2));
	vtx->x=vtx->x+r*sintheta*cos(phi);
	vtx->y=vtx->y+r*sintheta*sin(phi);
	vtx->z=vtx->z+r*costheta;


//distance with neighbours check
    for(i=0;i<vtx->neigh_no;i++){
        dist=vtx_distance_sq(vtx,vtx->neigh[i]);
        if(dist<1.0 || dist>vesicle->dmax) {
		    vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
    		return TS_FAIL;
		}
    }

// Distance with grafted poly-vertex check:	
	if(vtx->grafted_poly!=NULL){
		dist=vtx_distance_sq(vtx,vtx->grafted_poly->vlist->vtx[0]);
        if(dist<1.0 || dist>vesicle->dmax) {
		vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
		return TS_FAIL;
		}
	}

// TODO: Maybe faster if checks only nucleus-neighboring cells
// Nucleus penetration check:
if(vesicle->R_nucleus>0.0){
	if (vtx->x*vtx->x + vtx->y*vtx->y + vtx->z*vtx->z < vesicle->R_nucleus){
		vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
		return TS_FAIL;
	}
} else if(vesicle->R_nucleusX>0.0){
//	fprintf(stderr,"DEBUG, (Rx, Ry,Rz)^2=(%f,%f,%f)\n",vesicle->R_nucleusX, vesicle->R_nucleusY, vesicle->R_nucleusZ);
	if ((vtx->x*vtx->x)/vesicle->R_nucleusX + vtx->y*vtx->y/vesicle->R_nucleusY + (vtx->z*vtx->z)/vesicle->R_nucleusZ < 1.0){
		vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
		return TS_FAIL;
	}

}

//self avoidance check with distant vertices
	cellidx=vertex_self_avoidance(vesicle, vtx);
	//check occupation number
	retval=cell_occupation_number_and_internal_proximity(vesicle->clist,cellidx,vtx);

    if(retval==TS_FAIL){
		vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
        return TS_FAIL;
    } 
   
 
//if all the tests are successful, then energy for vtx and neighbours is calculated
	for(i=0;i<vtx->neigh_no;i++){
	memcpy((void *)&backupvtx[i+1],(void *)vtx->neigh[i],sizeof(ts_vertex));
	}

	if(vesicle->pswitch == 1 || vesicle->tape->constvolswitch>0){
		for(i=0;i<vtx->tristar_no;i++) dvol-=vtx->tristar[i]->volume;
	}

    if(vesicle->tape->constareaswitch==2){
		for(i=0;i<vtx->tristar_no;i++) darea-=vtx->tristar[i]->area;
    
    }

    delta_energy=0;
    
//    vesicle_volume(vesicle);
//    fprintf(stderr,"Volume in the beginning=%1.16e\n", vesicle->volume);

    //update the normals of triangles that share bead i.
    for(i=0;i<vtx->tristar_no;i++) triangle_normal_vector(vtx->tristar[i]);
	oenergy=vtx->energy;
    energy_vertex(vtx);
    delta_energy=vtx->xk*(vtx->energy - oenergy);
    //the same is done for neighbouring vertices
    for(i=0;i<vtx->neigh_no;i++){
        oenergy=vtx->neigh[i]->energy;
        energy_vertex(vtx->neigh[i]);
        delta_energy+=vtx->neigh[i]->xk*(vtx->neigh[i]->energy-oenergy);
    }

	if(vesicle->pswitch == 1 || vesicle->tape->constvolswitch >0){
		for(i=0;i<vtx->tristar_no;i++) dvol+=vtx->tristar[i]->volume;
        if(vesicle->pswitch==1) delta_energy-=vesicle->pressure*dvol;
	};

    if(vesicle->tape->constareaswitch==2){
        /* check whether the darea is gt epsarea */
		for(i=0;i<vtx->tristar_no;i++) darea+=vtx->tristar[i]->area;
        if(fabs(vesicle->area+darea-A0)>epsarea){
	        //restore old state.
 			vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
	        	for(i=0;i<vtx->neigh_no;i++){
		        	vtx->neigh[i]=memcpy((void *)vtx->neigh[i],(void *)&backupvtx[i+1],sizeof(ts_vertex));
	        	}
            		for(i=0;i<vtx->tristar_no;i++) triangle_normal_vector(vtx->tristar[i]); 
            		//fprintf(stderr,"fajlam!\n");
            		return TS_FAIL;
		}


    }

	if(vesicle->tape->constvolswitch==2){
		/*check whether the dvol is gt than epsvol */
			//fprintf(stderr,"DVOL=%1.16e\n",dvol);
		if(fabs(vesicle->volume+dvol-V0)>epsvol){
			//restore old state.
 			vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
	        	for(i=0;i<vtx->neigh_no;i++){
		        	vtx->neigh[i]=memcpy((void *)vtx->neigh[i],(void *)&backupvtx[i+1],sizeof(ts_vertex));
	        	}
            		for(i=0;i<vtx->tristar_no;i++) triangle_normal_vector(vtx->tristar[i]); 
            		//fprintf(stderr,"fajlam!\n");
            		return TS_FAIL;
		}

	} else
//    vesicle_volume(vesicle);
//    fprintf(stderr,"Volume before=%1.16e\n", vesicle->volume);
   if(vesicle->tape->constvolswitch == 1){
        retval=constvolume(vesicle, vtx, -dvol, &delta_energy_cv, &constvol_vtx_moved,&constvol_vtx_backup);
        if(retval==TS_FAIL){ // if we couldn't move the vertex to assure constant volume
            vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
	        for(i=0;i<vtx->neigh_no;i++){
		        vtx->neigh[i]=memcpy((void *)vtx->neigh[i],(void *)&backupvtx[i+1],sizeof(ts_vertex));
	        }
            for(i=0;i<vtx->tristar_no;i++) triangle_normal_vector(vtx->tristar[i]); 
 //           fprintf(stderr,"fajlam!\n");
            return TS_FAIL;
        }
//    vesicle_volume(vesicle);
//    fprintf(stderr,"Volume after=%1.16e\n", vesicle->volume);
//    fprintf(stderr,"Volume after-dvol=%1.16e\n", vesicle->volume-dvol);
//    fprintf(stderr,"Denergy before=%e\n",delta_energy);
    
    delta_energy+=delta_energy_cv;
//    fprintf(stderr,"Denergy after=%e\n",delta_energy);
    }
/* No poly-bond energy for now!
	if(vtx->grafted_poly!=NULL){
		delta_energy+=
			(pow(sqrt(vtx_distance_sq(vtx, vtx->grafted_poly->vlist->vtx[0])-1),2)-
			pow(sqrt(vtx_distance_sq(&backupvtx[0], vtx->grafted_poly->vlist->vtx[0])-1),2)) *vtx->grafted_poly->k;
	}
*/
//   fprintf(stderr, "DE=%f\n",delta_energy);
    //MONTE CARLOOOOOOOO
    if(delta_energy>=0){
#ifdef TS_DOUBLE_DOUBLE
        if(exp(-delta_energy)< drand48())
#endif
#ifdef TS_DOUBLE_FLOAT
        if(expf(-delta_energy)< (ts_float)drand48())
#endif
#ifdef TS_DOUBLE_LONGDOUBLE
        if(expl(-delta_energy)< (ts_ldouble)drand48())
#endif
    {
    //not accepted, reverting changes
  //  fprintf(stderr,"MC failed\n");
	vtx=memcpy((void *)vtx,(void *)&backupvtx[0],sizeof(ts_vertex));
	for(i=0;i<vtx->neigh_no;i++){
		vtx->neigh[i]=memcpy((void *)vtx->neigh[i],(void *)&backupvtx[i+1],sizeof(ts_vertex));
	}
	
    //update the normals of triangles that share bead i.
   for(i=0;i<vtx->tristar_no;i++) triangle_normal_vector(vtx->tristar[i]);

//    fprintf(stderr, "before vtx(x,y,z)=%e,%e,%e\n",constvol_vtx_moved->x, constvol_vtx_moved->y, constvol_vtx_moved->z);
    if(vesicle->tape->constvolswitch == 1){
        constvolumerestore(constvol_vtx_moved,constvol_vtx_backup);
    }
//    fprintf(stderr, "after vtx(x,y,z)=%e,%e,%e\n",constvol_vtx_moved->x, constvol_vtx_moved->y, constvol_vtx_moved->z);
//    vesicle_volume(vesicle);
//    fprintf(stderr,"Volume after fail=%1.16e\n", vesicle->volume);
    return TS_FAIL; 
    }
}
	//accepted	
 //   fprintf(stderr,"MC accepted\n");
//	oldcellidx=vertex_self_avoidance(vesicle, &backupvtx[0]);
	if(vtx->cell!=vesicle->clist->cell[cellidx]){
		retval=cell_add_vertex(vesicle->clist->cell[cellidx],vtx);
//		if(retval==TS_SUCCESS) cell_remove_vertex(vesicle->clist->cell[oldcellidx],vtx);
		if(retval==TS_SUCCESS) cell_remove_vertex(backupvtx[0].cell,vtx);
		
	}

    if(vesicle->tape->constvolswitch == 2){
	vesicle->volume+=dvol;
    } else
    if(vesicle->tape->constvolswitch == 1){
        constvolumeaccept(vesicle,constvol_vtx_moved,constvol_vtx_backup);
    }

    if(vesicle->tape->constareaswitch==2){
        vesicle->area+=darea;
    }
//	if(oldcellidx);
    //END MONTE CARLOOOOOOO
//    vesicle_volume(vesicle);
//    fprintf(stderr,"Volume after success=%1.16e\n", vesicle->volume);
    return TS_SUCCESS;
}


ts_bool single_poly_vertex_move(ts_vesicle *vesicle,ts_poly *poly,ts_vertex *vtx,ts_double *rn){
	ts_uint i;
	ts_bool retval; 
	ts_uint cellidx; 
//	ts_double delta_energy;
	ts_double costheta,sintheta,phi,r;
	ts_double dist;
	//This will hold all the information of vtx and its neighbours
	ts_vertex backupvtx;
//	ts_bond backupbond[2];
	memcpy((void *)&backupvtx,(void *)vtx,sizeof(ts_vertex));

	//random move in a sphere with radius stepsize:
	r=vesicle->stepsize*rn[0];
	phi=rn[1]*2*M_PI;
	costheta=2*rn[2]-1;
	sintheta=sqrt(1-pow(costheta,2));
	vtx->x=vtx->x+r*sintheta*cos(phi);
	vtx->y=vtx->y+r*sintheta*sin(phi);
	vtx->z=vtx->z+r*costheta;


	//distance with neighbours check
	for(i=0;i<vtx->neigh_no;i++){
		dist=vtx_distance_sq(vtx,vtx->neigh[i]);
		if(dist<1.0 || dist>vesicle->dmax) {
			vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
			return TS_FAIL;
		}
	}

// Distance with grafted vesicle-vertex check:	
	if(vtx==poly->vlist->vtx[0]){
		dist=vtx_distance_sq(vtx,poly->grafted_vtx);
        if(dist<1.0 || dist>vesicle->dmax) {
		vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
		return TS_FAIL;
		}
	}


	//self avoidance check with distant vertices
	cellidx=vertex_self_avoidance(vesicle, vtx);
	//check occupation number
	retval=cell_occupation_number_and_internal_proximity(vesicle->clist,cellidx,vtx);
	
	if(retval==TS_FAIL){
		vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
        return TS_FAIL;
	} 


	//if all the tests are successful, then energy for vtx and neighbours is calculated
/* Energy ignored for now!
	delta_energy=0;
	for(i=0;i<vtx->bond_no;i++){
		memcpy((void *)&backupbond[i],(void *)vtx->bond[i],sizeof(ts_bond));

		vtx->bond[i]->bond_length=sqrt(vtx_distance_sq(vtx->bond[i]->vtx1,vtx->bond[i]->vtx2));
		bond_energy(vtx->bond[i],poly);
		delta_energy+= vtx->bond[i]->energy - backupbond[i].energy;
	}

	if(vtx==poly->vlist->vtx[0]){
		delta_energy+=
			(pow(sqrt(vtx_distance_sq(vtx, poly->grafted_vtx)-1),2)-
			pow(sqrt(vtx_distance_sq(&backupvtx, poly->grafted_vtx)-1),2)) *poly->k;
		
	}


	if(delta_energy>=0){
#ifdef TS_DOUBLE_DOUBLE
        if(exp(-delta_energy)< drand48() )
#endif
#ifdef TS_DOUBLE_FLOAT
        if(expf(-delta_energy)< (ts_float)drand48())
#endif
#ifdef TS_DOUBLE_LONGDOUBLE
        if(expl(-delta_energy)< (ts_ldouble)drand48())
#endif
    	{
	//not accepted, reverting changes
	vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
	for(i=0;i<vtx->bond_no;i++){
	vtx->bond[i]=memcpy((void *)vtx->bond[i],(void *)&backupbond[i],sizeof(ts_bond));
	}

    return TS_FAIL; 
	}
	}
*/
		
//	oldcellidx=vertex_self_avoidance(vesicle, &backupvtx[0]);
	if(vtx->cell!=vesicle->clist->cell[cellidx]){
		retval=cell_add_vertex(vesicle->clist->cell[cellidx],vtx);
//		if(retval==TS_SUCCESS) cell_remove_vertex(vesicle->clist->cell[oldcellidx],vtx);
		if(retval==TS_SUCCESS) cell_remove_vertex(backupvtx.cell,vtx);	
	}
//	if(oldcellidx);
    //END MONTE CARLOOOOOOO
    return TS_SUCCESS;
}




ts_bool single_filament_vertex_move(ts_vesicle *vesicle,ts_poly *poly,ts_vertex *vtx,ts_double *rn){
	ts_uint i;
	ts_bool retval; 
	ts_uint cellidx; 
	ts_double delta_energy;
	ts_double costheta,sintheta,phi,r;
	ts_double dist[2];
	//This will hold all the information of vtx and its neighbours
	ts_vertex backupvtx,backupneigh[2];
	ts_bond backupbond[2];

	//backup vertex:		
	memcpy((void *)&backupvtx,(void *)vtx,sizeof(ts_vertex));

	//random move in a sphere with radius stepsize:
	r=vesicle->stepsize*rn[0];
	phi=rn[1]*2*M_PI;
	costheta=2*rn[2]-1;
	sintheta=sqrt(1-pow(costheta,2));
	vtx->x=vtx->x+r*sintheta*cos(phi);
	vtx->y=vtx->y+r*sintheta*sin(phi);
	vtx->z=vtx->z+r*costheta;


	//distance with neighbours check
	for(i=0;i<vtx->bond_no;i++){
		dist[i]=vtx_distance_sq(vtx->bond[i]->vtx1,vtx->bond[i]->vtx2);
		if(dist[i]<1.0 || dist[i]>vesicle->dmax) {
			vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
			return TS_FAIL;
		}
	}

// TODO: Maybe faster if checks only nucleus-neighboring cells
// Nucleus penetration check:
	if (vtx->x*vtx->x + vtx->y*vtx->y + vtx->z*vtx->z < vesicle->R_nucleus){
		vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
		return TS_FAIL;
	}


	//self avoidance check with distant vertices
	cellidx=vertex_self_avoidance(vesicle, vtx);
	//check occupation number
	retval=cell_occupation_number_and_internal_proximity(vesicle->clist,cellidx,vtx);
	if(retval==TS_FAIL){
		vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
        return TS_FAIL;
	} 

	//backup bonds
	for(i=0;i<vtx->bond_no;i++){
		memcpy(&backupbond[i],vtx->bond[i], sizeof(ts_bond));
		vtx->bond[i]->bond_length=sqrt(dist[i]);
		bond_vector(vtx->bond[i]);
	}

	//backup neighboring vertices:
	for(i=0;i<vtx->neigh_no;i++){
		memcpy(&backupneigh[i],vtx->neigh[i], sizeof(ts_vertex));
	}
	
	//if all the tests are successful, then energy for vtx and neighbours is calculated
	delta_energy=0;
	
	if(vtx->bond_no == 2){
		vtx->energy = -(vtx->bond[0]->x*vtx->bond[1]->x + vtx->bond[0]->y*vtx->bond[1]->y + vtx->bond[0]->z*vtx->bond[1]->z)/vtx->bond[0]->bond_length/vtx->bond[1]->bond_length;
		delta_energy += vtx->energy - backupvtx.energy;
	}

	for(i=0;i<vtx->neigh_no;i++){
		if(vtx->neigh[i]->bond_no == 2){
			vtx->neigh[i]->energy = -(vtx->neigh[i]->bond[0]->x*vtx->neigh[i]->bond[1]->x + vtx->neigh[i]->bond[0]->y*vtx->neigh[i]->bond[1]->y + vtx->neigh[i]->bond[0]->z*vtx->neigh[i]->bond[1]->z)/vtx->neigh[i]->bond[0]->bond_length/vtx->neigh[i]->bond[1]->bond_length;
			delta_energy += vtx->neigh[i]->energy - backupneigh[i].energy;
		}
	}

	// poly->k is filament persistence length (in units l_min)
	delta_energy *= poly->k;

	if(delta_energy>=0){
#ifdef TS_DOUBLE_DOUBLE
        if(exp(-delta_energy)< drand48() )
#endif
#ifdef TS_DOUBLE_FLOAT
        if(expf(-delta_energy)< (ts_float)drand48())
#endif
#ifdef TS_DOUBLE_LONGDOUBLE
        if(expl(-delta_energy)< (ts_ldouble)drand48())
#endif
    	{
	//not accepted, reverting changes
	vtx=memcpy((void *)vtx,(void *)&backupvtx,sizeof(ts_vertex));
	for(i=0;i<vtx->neigh_no;i++){
		memcpy(vtx->neigh[i],&backupneigh[i],sizeof(ts_vertex));
	}
	for(i=0;i<vtx->bond_no;i++){
		vtx->bond[i]=memcpy((void *)vtx->bond[i],(void *)&backupbond[i],sizeof(ts_bond));
	}

    return TS_FAIL; 
	}
	}
	
	
//	oldcellidx=vertex_self_avoidance(vesicle, &backupvtx[0]);
	if(vtx->cell!=vesicle->clist->cell[cellidx]){
		retval=cell_add_vertex(vesicle->clist->cell[cellidx],vtx);
//		if(retval==TS_SUCCESS) cell_remove_vertex(vesicle->clist->cell[oldcellidx],vtx);
		if(retval==TS_SUCCESS) cell_remove_vertex(backupvtx.cell,vtx);	
	}
//	if(oldcellidx);
    //END MONTE CARLOOOOOOO
    return TS_SUCCESS;
}
