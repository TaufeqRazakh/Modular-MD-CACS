/*----------------------------------------------------------------------
Program pmd.c performs parallel molecular-dynamics for Lennard-Jones 
systems using the Message Passing Interface (MPI) standard.
----------------------------------------------------------------------*/
#include <iostream>
#include <cstdlib>
#include <cmath>
#include <fstream>      // std::fstream
#include <vector>
#include <array>
#include "pmd.hpp"

const double ARmass = 39.94800000; //A.U.s
const double ARsigma = 3.40500000; // Angstroms
const double AReps   = 119.800000; // Kelvins
const double CellDim = 12.0000000; // Angstroms
int NPartPerCell = 10;

using namespace std;

// Class for keeping track of the properties for an atom
class Atom{
public:
  double x;		// position in x axis
  double y;		// position in y axis
  double z;		// position in y axis

  double ax;		// acceleration on x axis
  double ay;		// acceleration on y axis
  double az;		// acceleration on y axis
  
  double vx;		// velocity on x axis
  double vy;		// velocity on y axis
  double vz;            // velocity on y axis
  // Default constructor
  Atom() 
    : x(0.0),y(0.0),z(0.0),
      ax(0.0),ay(0.0),az(0.0),vx(0.0),vy(0.0),vz(0.0){
  }
  
  double update(){
    // We are using a 1.0 fs timestep, this is converted
    double DT = 0.000911633;
    ax = fx/ARmass;
    ay = fy/ARmass;
    az = fz/ARmass;
    vx += ax*0.5*DT;
    vy += ay*0.5*DT;
    vz += ay*0.5*DT;
    x += DT*vx;
    y += DT*vy;
    z += DT*vz;
    fx = 0.0;
    fy = 0.0;
    fz = 0.0;
    return 0.5*ARmass*(vx*vx+vy*vy+vz*vz);
  }
};
 

class Cell {
public:
  
  int pid; //sequential processor ID of this cell
  MPI_Comm_rank(MPI_COMM_WORLD, &pid);

  /* Vector index of this processor */
  array<int, 3> vid;
  array<int, 3> myparity;
  array<double, 3> vSum, gvSum, vMag;
  vector<Atom> atoms;

  Cell(array<int, 3> InitUcell, double InitTemp) {

    array<double, 3> c,gap;
    int j,a,nX,nY,nZ;

      /* FCC atoms in the original unit cell */
    vector<vector<double> > = {{0.0, 0.0, 0.0}, {0.0, 0.5, 0.5},
                           {0.5, 0.0, 0.5}, {0.5, 0.5, 0.0}};

    /* Set up a face-centered cubic (fcc) lattice */
    for (a=0; a<3; a++) gap[a] = al[a]/InitUcell[a];

    for (nZ=0; nZ<InitUcell[2]; nZ++) {
      c[2] = nZ*gap[2];
      for (nY=0; nY<InitUcell[1]; nY++) {
	c[1] = nY*gap[1];
	for (nX=0; nX<InitUcell[0]; nX++) {
	  c[0] = nX*gap[0];
	  for (j=0; j<4; j++) {
	    Atom atom;
	    atom.x = c[0] + gap[0]*origAtom[j][0];
	    atom.vx = sqrt(3*InitTemp)*drand48();
	    atom.y = c[1] + gap[1]*origAtom[j][1];
	    atom.vy = sqrt(3*InitTemp)*drand48();
	    atom.z = c[2] + gap[2]*origAtom[j][2];
	    atom.vz = sqrt(3*InitTemp)*drand48();
	    atoms.push_back(atom);
	  }
	}
      }
    }
    
    absx = ax;
    absy = ay;
    absz = az;
    //  We will be filling the cells, making sure than
    //  No atom is less than 2 Angstroms from another
    double rcelldim = double(CellDim);
    double centerx = rcelldim*double(ax) + 0.5*rcelldim;
    double centery = rcelldim*double(ay) + 0.5*rcelldim;
    double centerz = rcelldim*double(az) + 0.5*rcelldim;
   
    // Randomly initialize particles to be some distance 
    // from the center of the square
    // place first atom in cell
    particles[0].x = centerx + ((drand48()-0.5)*(CellDim-2));
    particles[0].y = centery + ((drand48()-0.5)*(CellDim-2));
    particles[0].z = centerz + ((drand48()-0.5)*(CellDim-2));
    
    double r,rx,ry;
    for (int i = 1; i < particles.size(); i++) {
      r = 0;
      while(r < 2.7) {   // square of 2
	r = 2.70001;
	rx = centerx + ((drand48()-0.5)*(CellDim-2)); 
	ry = centery + ((drand48()-0.5)*(CellDim-2));
      	//loop over all other current atoms
	for(int j = 0; j<i; j++){
	  double rxj = rx - particles[j].x;
	  double ryj = ry - particles[j].y;
	  double rzj = ry - particles[j].z;
	  double rt = rxj*rxj+ryj*ryj+rzj*rzj;
	  if(rt < r) r=rt;
	}
      }
      particles[i].x = rx;
      particles[i].y = ry;
      particles[i].z = rz;
    }
  }

  void init_neighbor_node(array<int, 3> vproc) {
    
    vid[0] = pid/(vproc[1]*vproc[2]);
    vid[1] = (pid/vproc[2])%vproc[1];
    vid[2] = pid%vproc[2];
    
    // Prepare neighbor-node ID table for a cell
    array<int, 6> nn;
    vector<vector<int > > iv = {
				{-1,0,0}, {1,0,0}, {0,-1,0}, {0,1,0}, {0,0,-1}, {0,0,1}
    };
    
    int ku, a, k1[3];
    
    /* Set up neighbor tables, nn & sv */
    for (ku=0; ku<6; ku++) {
      /* Vector index of neighbor ku */
      for (a=0; a<3; a++)
	k1[a] = (vid[a]+iv[ku][a]+vproc[a])%vproc[a];
      /* Scalar neighbor ID, nn */
      nn[ku] = k1[0]*vproc[1]*vproc[2]+k1[1]*vproc[2]+k1[2];
      /* Shift vector, sv */
      for (a=0; a<3; a++) sv[ku][a] = al[a]*iv[ku][a];
    }
    
    // Set up node parity table
    for (a=0; a<3; a++) {
      if (vproc[a] == 1) 
	myparity[a] = 2;
      else if (vid[a]%2 == 0)
	myparity[a] = 0;
      else
	myparity[a] = 1;
    }
  }
  
  
  void Communicate(int*** CellMap, int Dimension){
    // Pack the message
    PartCoordsBuff[0] = particles.size();
    for(int i=0;i<particles.size();i++){
      PartCoordsBuff[1+i*2] = particles[i].x;
      PartCoordsBuff[1+i*2+1] = particles[i].y;
    }
    
    // Assuming a good MPI layer, communication between two of the same 
    // processor should be pretty efficient.  Not screening greatly simplifies
    // the code
    
    // i+1,j
    int ircount =0;
    if(absx!=Dimension-1){
      int tag = (absx+1)*Dimension+absy;
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
      		MPI_DOUBLE,CellMap[absx+1][absy][0],
      		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    //i-1,j
    if(absx!= 0){
      int tag = (absx-1)*Dimension+absy;
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx-1][absy][0],
		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    // i,j+1
    if(absy!=Dimension-1){
      int tag = (absx)*Dimension+(absy+1);
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx][absy+1][0],
		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    // i,j-1
    if(absy !=0){
      int tag = absx*Dimension+(absy-1);
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx][absy-1][0],
		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    // i+1,j+1
    if(absx != Dimension-1 && absy != Dimension-1){
      int tag = (absx+1)*Dimension + (absy+1);
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx+1][absy+1][0],
		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    //i+1,j-1
    if(absx != Dimension-1 && absy != 0){ 
      int tag = (absx+1)*Dimension + (absy-1);
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx+1][absy-1][0],
		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    //i-1,j+1
    if(absx != 0 && absy != Dimension-1){
      int tag = (absx-1)*Dimension + (absy+1);
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx-1][absy+1][0],
		tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    //i-1,j-1
    if(absx !=0 && absy !=0){
      int tag = (absx-1)*Dimension + (absy-1);
      MPI_Irecv(remote_particles[ircount],NPartPerCell*2+1,
		MPI_DOUBLE,CellMap[absx-1][absy-1][0],
	       	tag,MPI_COMM_WORLD,&reqr[ircount]);
      ircount++;
    }
    
    // Now the sending
    int iscount = 0;

    //i+1,j   
    if(absx != Dimension-1){
      int tag = absx*Dimension + absy;
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx+1][absy][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    // i-1,j
    if(absx != 0){
      int tag = absx*Dimension + absy;
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx-1][absy][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    // i,j+1
    if(absy != Dimension-1){
      int tag = (absx)*Dimension + (absy);
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx][absy+1][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    // i,j-1
    if(absy != 0){
      int tag = (absx)*Dimension + (absy);
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx][absy-1][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    // i+1,j+1
    if(absx != Dimension-1 && absy != Dimension-1){
      int tag = (absx)*Dimension + (absy);
       MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx+1][absy+1][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    //i+1,j-1
    if(absx!=Dimension-1 && absy!=0){
      int tag = (absx)*Dimension + (absy);
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx+1][absy-1][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    //i-1,j+1
    if(absx!=0 && absy!= Dimension-1){
      int tag = (absx)*Dimension + (absy);
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx-1][absy+1][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    //i-1,j-1
    if(absx!=0 && absy!=0){
      int tag = (absx)*Dimension + (absy);
      MPI_Isend(PartCoordsBuff,NPartPerCell*2+1,MPI_DOUBLE,CellMap[absx-1][absy-1][0],
		tag,MPI_COMM_WORLD,&reqs[iscount]);
      iscount++;
    }
    nreqr = ircount;
    nreqs = iscount;
  }
  void PostWaits(void){
    MPI_Status statr[8];
    MPI_Status stats[8];
    MPI_Waitall(nreqs,reqs,stats);
    MPI_Waitall(nreqr,reqr,statr);
  }
};
    
//Force calculation
//------------------------------------
// Overloaded to handle new case
double interact(Particle &atom1, double atom2x, double atom2y. double atom2z){
  double rx,ry,rz,r,fx,fy,fz,f;
  double sigma6,sigma12;
  
  // computing base values
  rx = atom1.x - atom2x;
  ry = atom1.y - atom2y;
  rz = atom1.z - atom2z;
  r = rx*rx + ry*ry +  rz*rz;

  if(r < 0.000001)
    return 0.0;
  
  r=sqrt(r);
  double sigma2 = (ARsigma/r)*(ARsigma/r);
  sigma6 = sigma2*sigma2*sigma2;
  sigma12 = sigma6*sigma6;
  f = ((sigma12-0.5*sigma6)*48.0*AReps)/r;
  fx = f * rx;
  fy = f * ry;
  fz = f * rz;
  // updating particle properties
  atom1.fx += fx;
  atom1.fy += fy;
  atom1.fz += fz;
  return 4.0*AReps*(sigma12-sigma6);
}

double interact(Particle &atom1, Particle &atom2){
  return interact(atom1,atom2.x,atom2.y,atom2.z);
}

int ComputeAtomsPerCell(int ***CellMap, 
			int NRows,int NCols, 
			int NParts){

  
  int max = NPartPerCell;
  for(int i=0;i<NRows;i++)
    for(int j=0;j<NCols;j++)
      CellMap[i][j][3] = NPartPerCell;
  
  int molsum = NRows*NCols*NPartPerCell;
  while(molsum < NParts){
    max++;
    for(int i=0;i<NRows;i++){
      for(int j=0;j<NCols;j++){
	CellMap[i][j][3]++;
	molsum++;
	if(molsum >= NParts) {
	  return max;
	}
      }
    }
  }
  return max;
}

/*--------------------------------------------------------------------*/
int main(int argc, char **argv) {
/*--------------------------------------------------------------------*/
  double cpu1;

  MPI_Init(&argc,&argv); /* Initialize the MPI environment */
  MPI_Comm_rank(MPI_COMM_WORLD, &sid);  /* My processor ID */

  init_params();
  // set_topology(); This is now implemented in the Cell Object. Each Object makes its neighbor tables
  init_conf();
  atom_copy();
  compute_accel(); /* Computes initial accelerations */ 

  cpu1 = MPI_Wtime();
  for (stepCount=1; stepCount<=StepLimit; stepCount++) {
    single_step(); 
    if (stepCount%StepAvg == 0) eval_props();
  }
  cpu = MPI_Wtime() - cpu1;
  if (sid == 0) printf("CPU & COMT = %le %le\n",cpu,comt);

  MPI_Finalize(); /* Clean up the MPI environment */
  return 0;
}

/*--------------------------------------------------------------------*/
void init_params() {
/*----------------------------------------------------------------------
Initializes parameters.
----------------------------------------------------------------------*/
  int a;
  double rr,ri2,ri6,r1;
  FILE *fp;

  /* Read control parameters */
   ifstream ifs("pmd.in", ifstream::in);
  if(!ifs.is_open()) {
    cerr << "failed to open input file" << endl;
    terminate();
  }

  ifs >> vproc[0] >> vproc[1] >> vproc[2];
  ifs >> InitUcell[0] >> InitUcell[1] >> InitUcell[2];
  ifs >> Density >> InitTemp >> DeltaT;
  ifs >> StepLimit >> StepAvg;

  ifs.close();
  
  /* Compute basic parameters */
  DeltaTH = 0.5*DeltaT;
  for (a=0; a<3; a++) al[a] = InitUcell[a]/pow(Density/4.0,1.0/3.0);
  if (sid == 0) printf("al = %e %e %e\n",al[0],al[1],al[2]);

  /* Compute the # of cells for linked cell lists */
  for (a=0; a<3; a++) {
    lc[a] = al[a]/RCUT; 
    rc[a] = al[a]/lc[a];
  }
  if (sid == 0) {
    printf("lc = %d %d %d\n",lc[0],lc[1],lc[2]);
    printf("rc = %e %e %e\n",rc[0],rc[1],rc[2]);
  }

  /* Constants for potential truncation */
  rr = RCUT*RCUT; ri2 = 1.0/rr; ri6 = ri2*ri2*ri2; r1=sqrt(rr);
  Uc = 4.0*ri6*(ri6 - 1.0);
  Duc = -48.0*ri6*(ri6 - 0.5)/r1;
}

/*--------------------------------------------------------------------*/
void init_conf() {
/*----------------------------------------------------------------------
r are initialized to face-centered cubic (fcc) lattice positions.  
rv are initialized with a random velocity corresponding to Temperature.  
----------------------------------------------------------------------*/
  double c[3],gap[3],e[3],vSum[3],gvSum[3],vMag;
  int j,a,nX,nY,nZ;
  double seed;

  /* Set up a face-centered cubic (fcc) lattice */
  for (a=0; a<3; a++) gap[a] = al[a]/InitUcell[a];
  n = 0;
  for (nZ=0; nZ<InitUcell[2]; nZ++) {
    c[2] = nZ*gap[2];
    for (nY=0; nY<InitUcell[1]; nY++) {
      c[1] = nY*gap[1];
      for (nX=0; nX<InitUcell[0]; nX++) {
        c[0] = nX*gap[0];
        for (j=0; j<4; j++) {
          for (a=0; a<3; a++)
            r[n][a] = c[a] + gap[a]*origAtom[j][a];
          ++n;
        }
      }
    }
  }

  vector<Cell> cells(InitUcell[0]*InitUcell[1]*InitUcell[2]);

  /* Generate random velocities */
  seed = 13597.0+sid;
  vMag = sqrt(3*InitTemp);
  for(a=0; a<3; a++) vSum[a] = 0.0;
  for(j=0; j<n; j++) {
    RandVec3(e,&seed);
    for (a=0; a<3; a++) {
      rv[j][a] = vMag*e[a];
      vSum[a] = vSum[a] + rv[j][a];
    }
  }
  // MPI_Allreduce(vSum,gvSum,3,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);

  /* Make the total momentum zero */
  for (a=0; a<3; a++) gvSum[a] /= nglob;
  for (j=0; j<n; j++)
    for(a=0; a<3; a++) rv[j][a] -= gvSum[a];
}

/*--------------------------------------------------------------------*/
void single_step() {
/*----------------------------------------------------------------------
r & rv are propagated by DeltaT using the velocity-Verlet scheme.
----------------------------------------------------------------------*/
  int i,a;

  half_kick(); /* First half kick to obtain v(t+Dt/2) */
  for (i=0; i<n; i++) /* Update atomic coordinates to r(t+Dt) */
    for (a=0; a<3; a++) r[i][a] = r[i][a] + DeltaT*rv[i][a];
  atom_move();
  atom_copy();
  compute_accel(); /* Computes new accelerations, a(t+Dt) */
  half_kick(); /* Second half kick to obtain v(t+Dt) */
}

/*--------------------------------------------------------------------*/
void half_kick() {
/*----------------------------------------------------------------------
Accelerates atomic velocities, rv, by half the time step.
----------------------------------------------------------------------*/
  int i,a;
  for (i=0; i<n; i++)
    for (a=0; a<3; a++) rv[i][a] = rv[i][a]+DeltaTH*ra[i][a];
}

/*--------------------------------------------------------------------*/
void atom_copy() {
/*----------------------------------------------------------------------
Exchanges boundary-atom coordinates among neighbor nodes:  Makes 
boundary-atom list, LSB, then sends & receives boundary atoms.
----------------------------------------------------------------------*/
  int kd,kdd,i,ku,inode,nsd,nrc,a;
  int nbnew = 0; /* # of "received" boundary atoms */
  double com1;

/* Main loop over x, y & z directions starts--------------------------*/

  for (kd=0; kd<3; kd++) {

    /* Make a boundary-atom list, LSB---------------------------------*/

    /* Reset the # of to-be-copied atoms for lower&higher directions */
    for (kdd=0; kdd<2; kdd++) lsb[2*kd+kdd][0] = 0;

    /* Scan all the residents & copies to identify boundary atoms */ 
    for (i=0; i<n+nbnew; i++) {
      for (kdd=0; kdd<2; kdd++) {
        ku = 2*kd+kdd; /* Neighbor ID */
        /* Add an atom to the boundary-atom list, LSB, for neighbor ku 
           according to bit-condition function, bbd */
        if (bbd(r[i],ku)) lsb[ku][++(lsb[ku][0])] = i;
      }
    }

    /* Message passing------------------------------------------------*/

    com1=MPI_Wtime(); /* To calculate the communication time */

    /* Loop over the lower & higher directions */
    for (kdd=0; kdd<2; kdd++) {

      inode = nn[ku=2*kd+kdd]; /* Neighbor node ID */

      /* Send & receive the # of boundary atoms-----------------------*/

      nsd = lsb[ku][0]; /* # of atoms to be sent */

      /* Even node: send & recv */
      if (myparity[kd] == 0) {
        MPI_Send(&nsd,1,MPI_INT,inode,10,MPI_COMM_WORLD);
        MPI_Recv(&nrc,1,MPI_INT,MPI_ANY_SOURCE,10,
                 MPI_COMM_WORLD,&status);
      }
      /* Odd node: recv & send */
      else if (myparity[kd] == 1) {
        MPI_Recv(&nrc,1,MPI_INT,MPI_ANY_SOURCE,10,
                 MPI_COMM_WORLD,&status);
        MPI_Send(&nsd,1,MPI_INT,inode,10,MPI_COMM_WORLD);
      }
      /* Single layer: Exchange information with myself */
      else
        nrc = nsd;
      /* Now nrc is the # of atoms to be received */

      /* Send & receive information on boundary atoms-----------------*/

      /* Message buffering */
      for (i=1; i<=nsd; i++)
        for (a=0; a<3; a++) /* Shift the coordinate origin */
          dbuf[3*(i-1)+a] = r[lsb[ku][i]][a]-sv[ku][a]; 

      /* Even node: send & recv */
      if (myparity[kd] == 0) {
        MPI_Send(dbuf,3*nsd,MPI_DOUBLE,inode,20,MPI_COMM_WORLD);
        MPI_Recv(dbufr,3*nrc,MPI_DOUBLE,MPI_ANY_SOURCE,20,
                 MPI_COMM_WORLD,&status);
      }
      /* Odd node: recv & send */
      else if (myparity[kd] == 1) {
        MPI_Recv(dbufr,3*nrc,MPI_DOUBLE,MPI_ANY_SOURCE,20,
                 MPI_COMM_WORLD,&status);
        MPI_Send(dbuf,3*nsd,MPI_DOUBLE,inode,20,MPI_COMM_WORLD);
      }
      /* Single layer: Exchange information with myself */
      else
        for (i=0; i<3*nrc; i++) dbufr[i] = dbuf[i];

      /* Message storing */
      for (i=0; i<nrc; i++)
        for (a=0; a<3; a++) r[n+nbnew+i][a] = dbufr[3*i+a]; 

      /* Increment the # of received boundary atoms */
      nbnew = nbnew+nrc;

      /* Internode synchronization */
      MPI_Barrier(MPI_COMM_WORLD);

    } /* Endfor lower & higher directions, kdd */

    comt += MPI_Wtime()-com1; /* Update communication time, COMT */

  } /* Endfor x, y & z directions, kd */

  /* Main loop over x, y & z directions ends--------------------------*/

  /* Update the # of received boundary atoms */
  nb = nbnew;
}

/*--------------------------------------------------------------------*/
void compute_accel() {
/*----------------------------------------------------------------------
Given atomic coordinates, r[0:n+nb-1][], for the extended (i.e., 
resident & copied) system, computes the acceleration, ra[0:n-1][], for 
the residents.
----------------------------------------------------------------------*/
  int i,j,a,lc2[3],lcyz2,lcxyz2,mc[3],c,mc1[3],c1;
  int bintra;
  double dr[3],rr,ri2,ri6,r1,rrCut,fcVal,f,vVal,lpe;

  /* Reset the potential & forces */
  lpe = 0.0;
  for (i=0; i<n; i++) for (a=0; a<3; a++) ra[i][a] = 0.0;

  /* Make a linked-cell list, lscl------------------------------------*/

  for (a=0; a<3; a++) lc2[a] = lc[a]+2;
  lcyz2 = lc2[1]*lc2[2];
  lcxyz2 = lc2[0]*lcyz2;

  /* Reset the headers, head */
  for (c=0; c<lcxyz2; c++) head[c] = EMPTY;

  /* Scan atoms to construct headers, head, & linked lists, lscl */

  for (i=0; i<n+nb; i++) {
    for (a=0; a<3; a++) mc[a] = (r[i][a]+rc[a])/rc[a];

    /* Translate the vector cell index, mc, to a scalar cell index */
    c = mc[0]*lcyz2+mc[1]*lc2[2]+mc[2];

    /* Link to the previous occupant (or EMPTY if you're the 1st) */
    lscl[i] = head[c];

    /* The last one goes to the header */
    head[c] = i;
  } /* Endfor atom i */

  /* Calculate pair interaction---------------------------------------*/

  rrCut = RCUT*RCUT;

  /* Scan inner cells */
  for (mc[0]=1; mc[0]<=lc[0]; (mc[0])++)
  for (mc[1]=1; mc[1]<=lc[1]; (mc[1])++)
  for (mc[2]=1; mc[2]<=lc[2]; (mc[2])++) {

    /* Calculate a scalar cell index */
    c = mc[0]*lcyz2+mc[1]*lc2[2]+mc[2];
    /* Skip this cell if empty */
    if (head[c] == EMPTY) continue;

    /* Scan the neighbor cells (including itself) of cell c */
    for (mc1[0]=mc[0]-1; mc1[0]<=mc[0]+1; (mc1[0])++)
    for (mc1[1]=mc[1]-1; mc1[1]<=mc[1]+1; (mc1[1])++)
    for (mc1[2]=mc[2]-1; mc1[2]<=mc[2]+1; (mc1[2])++) {

      /* Calculate the scalar cell index of the neighbor cell */
      c1 = mc1[0]*lcyz2+mc1[1]*lc2[2]+mc1[2];
      /* Skip this neighbor cell if empty */
      if (head[c1] == EMPTY) continue;

      /* Scan atom i in cell c */
      i = head[c];
      while (i != EMPTY) {

        /* Scan atom j in cell c1 */
        j = head[c1];
        while (j != EMPTY) {

          /* No calculation with itself */
          if (j != i) {
            /* Logical flag: intra(true)- or inter(false)-pair atom */
            bintra = (j < n);

            /* Pair vector dr = r[i] - r[j] */
            for (rr=0.0, a=0; a<3; a++) {
              dr[a] = r[i][a]-r[j][a];
              rr += dr[a]*dr[a];
            }

            /* Calculate potential & forces for intranode pairs (i < j)
               & all the internode pairs if rij < RCUT; note that for
               any copied atom, i < j */
            if (i<j && rr<rrCut) {
              ri2 = 1.0/rr; ri6 = ri2*ri2*ri2; r1 = sqrt(rr);
              fcVal = 48.0*ri2*ri6*(ri6-0.5) + Duc/r1;
              vVal = 4.0*ri6*(ri6-1.0) - Uc - Duc*(r1-RCUT);
              if (bintra) lpe += vVal; else lpe += 0.5*vVal;
              for (a=0; a<3; a++) {
                f = fcVal*dr[a];
                ra[i][a] += f;
                if (bintra) ra[j][a] -= f;
              }
            }
          } /* Endif not self */
          
          j = lscl[j];
        } /* Endwhile j not empty */

        i = lscl[i];
      } /* Endwhile i not empty */

    } /* Endfor neighbor cells, c1 */

  } /* Endfor central cell, c */

  /* Global potential energy */
  MPI_Allreduce(&lpe,&potEnergy,1,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);
}

/*--------------------------------------------------------------------*/
void eval_props() {
/*----------------------------------------------------------------------
Evaluates physical properties: kinetic, potential & total energies.
----------------------------------------------------------------------*/
  double vv,lke;
  int i,a;

  /* Total kinetic energy */
  for (lke=0.0, i=0; i<n; i++) {
    for (vv=0.0, a=0; a<3; a++) vv += rv[i][a]*rv[i][a];
    lke += vv;
  }
  lke *= 0.5;
  MPI_Allreduce(&lke,&kinEnergy,1,MPI_DOUBLE,MPI_SUM,MPI_COMM_WORLD);

  /* Energy paer atom */
  kinEnergy /= nglob;
  potEnergy /= nglob;
  totEnergy = kinEnergy + potEnergy;
  temperature = kinEnergy*2.0/3.0;

  /* Print the computed properties */
  if (sid == 0) printf("%9.6f %9.6f %9.6f %9.6f\n",
                stepCount*DeltaT,temperature,potEnergy,totEnergy);
}

/*--------------------------------------------------------------------*/
void atom_move() {
/*----------------------------------------------------------------------
Sends moved-out atoms to neighbor nodes and receives moved-in atoms 
from neighbor nodes.  Called with n, r[0:n-1] & rv[0:n-1], atom_move 
returns a new n' together with r[0:n'-1] & rv[0:n'-1].
----------------------------------------------------------------------*/

/* Local variables------------------------------------------------------

mvque[6][NBMAX]: mvque[ku][0] is the # of to-be-moved atoms to neighbor 
  ku; MVQUE[ku][k>0] is the atom ID, used in r, of the k-th atom to be
  moved.
----------------------------------------------------------------------*/
  int mvque[6][NBMAX];
  int newim = 0; /* # of new immigrants */
  int ku,kd,i,kdd,kul,kuh,inode,ipt,a,nsd,nrc;
  double com1;

  /* Reset the # of to-be-moved atoms, MVQUE[][0] */
  for (ku=0; ku<6; ku++) mvque[ku][0] = 0;

  /* Main loop over x, y & z directions starts------------------------*/

  for (kd=0; kd<3; kd++) {

    /* Make a moved-atom list, mvque----------------------------------*/

    /* Scan all the residents & immigrants to list moved-out atoms */
    for (i=0; i<n+newim; i++) {
      kul = 2*kd  ; /* Neighbor ID */
      kuh = 2*kd+1; 
      /* Register a to-be-copied atom in mvque[kul|kuh][] */      
      if (r[i][0] > MOVED_OUT) { /* Don't scan moved-out atoms */
        /* Move to the lower direction */
        if (bmv(r[i],kul)) mvque[kul][++(mvque[kul][0])] = i;
        /* Move to the higher direction */
        else if (bmv(r[i],kuh)) mvque[kuh][++(mvque[kuh][0])] = i;
      }
    }

    /* Message passing with neighbor nodes----------------------------*/

    com1 = MPI_Wtime();

    /* Loop over the lower & higher directions------------------------*/

    for (kdd=0; kdd<2; kdd++) {

      inode = nn[ku=2*kd+kdd]; /* Neighbor node ID */

      /* Send atom-number information---------------------------------*/  

      nsd = mvque[ku][0]; /* # of atoms to-be-sent */

      /* Even node: send & recv */
      if (myparity[kd] == 0) {
        MPI_Send(&nsd,1,MPI_INT,inode,110,MPI_COMM_WORLD);
        MPI_Recv(&nrc,1,MPI_INT,MPI_ANY_SOURCE,110,
                 MPI_COMM_WORLD,&status);
      }
      /* Odd node: recv & send */
      else if (myparity[kd] == 1) {
        MPI_Recv(&nrc,1,MPI_INT,MPI_ANY_SOURCE,110,
                 MPI_COMM_WORLD,&status);
        MPI_Send(&nsd,1,MPI_INT,inode,110,MPI_COMM_WORLD);
      }
      /* Single layer: Exchange information with myself */
      else
        nrc = nsd;
      /* Now nrc is the # of atoms to be received */

      /* Send & receive information on boundary atoms-----------------*/

      /* Message buffering */
      for (i=1; i<=nsd; i++)
        for (a=0; a<3; a++) {
          /* Shift the coordinate origin */
          dbuf[6*(i-1)  +a] = r [mvque[ku][i]][a]-sv[ku][a]; 
          dbuf[6*(i-1)+3+a] = rv[mvque[ku][i]][a];
          r[mvque[ku][i]][0] = MOVED_OUT; /* Mark the moved-out atom */
        }

      /* Even node: send & recv, if not empty */
      if (myparity[kd] == 0) {
        MPI_Send(dbuf,6*nsd,MPI_DOUBLE,inode,120,MPI_COMM_WORLD);
        MPI_Recv(dbufr,6*nrc,MPI_DOUBLE,MPI_ANY_SOURCE,120,
                 MPI_COMM_WORLD,&status);
      }
      /* Odd node: recv & send, if not empty */
      else if (myparity[kd] == 1) {
        MPI_Recv(dbufr,6*nrc,MPI_DOUBLE,MPI_ANY_SOURCE,120,
                 MPI_COMM_WORLD,&status);
        MPI_Send(dbuf,6*nsd,MPI_DOUBLE,inode,120,MPI_COMM_WORLD);
      }
      /* Single layer: Exchange information with myself */
      else
        for (i=0; i<6*nrc; i++) dbufr[i] = dbuf[i];

      /* Message storing */
      for (i=0; i<nrc; i++)
        for (a=0; a<3; a++) {
          r [n+newim+i][a] = dbufr[6*i  +a]; 
          rv[n+newim+i][a] = dbufr[6*i+3+a];
        }

      /* Increment the # of new immigrants */
      newim = newim+nrc;

      /* Internode synchronization */
      MPI_Barrier(MPI_COMM_WORLD);

    } /* Endfor lower & higher directions, kdd */

    comt=comt+MPI_Wtime()-com1;

  } /* Endfor x, y & z directions, kd */
  
  /* Main loop over x, y & z directions ends--------------------------*/

  /* Compress resident arrays including new immigrants */

  ipt = 0;
  for (i=0; i<n+newim; i++) {
    if (r[i][0] > MOVED_OUT) {
      for (a=0; a<3; a++) {
        r [ipt][a] = r [i][a];
        rv[ipt][a] = rv[i][a];
      }
      ++ipt;
    }
  }

  /* Update the compressed # of resident atoms */
  n = ipt;
}

/*----------------------------------------------------------------------
Bit condition functions:

1. bbd(ri,ku) is .true. if coordinate ri[3] is in the boundary to 
     neighbor ku.
2. bmv(ri,ku) is .true. if an atom with coordinate ri[3] has moved out 
     to neighbor ku.
----------------------------------------------------------------------*/
int bbd(double* ri, int ku) {
  int kd,kdd;
  kd = ku/2; /* x(0)|y(1)|z(2) direction */
  kdd = ku%2; /* Lower(0)|higher(1) direction */
  if (kdd == 0)
    return ri[kd] < RCUT;
  else
    return al[kd]-RCUT < ri[kd];
}
int bmv(double* ri, int ku) {
  int kd,kdd;
  kd = ku/2; /* x(0)|y(1)|z(2) direction */
  kdd = ku%2; /* Lower(0)|higher(1) direction */
  if (kdd == 0)
    return ri[kd] < 0.0;
  else
    return al[kd] < ri[kd];
}
