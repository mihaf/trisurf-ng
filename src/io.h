#ifndef _IO_H
#define _IO_H

/** @ Global variables for I/O operations like filenames etc. */
/*static char mastername[1024];
static char prefixname[1024];
static ts_bool restore=0;
static char tape[1024]; */
int force_from_tape;


typedef struct {
	long int nshell;
	long int ncxmax;
	long int ncymax;
	long int nczmax;
	long int npoly;
	long int nmono;
	long int nfil;
	long int nfono;
	long int R_nucleus;
	long int pswitch;
    	char *multiprocessing;
   	long int brezveze0;
    	long int brezveze1;
    	long int brezveze2;
    	ts_double xk0;
	ts_double dmax;
	ts_double stepsize;
	ts_double kspring;
	ts_double xi;
	ts_double pressure;
	long int iterations;
	long int inititer;
	long int mcsweeps;
	long int quiet;
} ts_tape;

typedef struct{
	ts_int force_from_tape;
	ts_int reset_iteration_count;
    char path[1024]; //path where all files should be added
    char output_fullfilename[1024]; //name of the master file
    char dump_fullfilename[1024]; //name of the dump file
    char tape_fullfilename[1024]; //name of the tape file
} ts_args;

ts_args command_line_args;

ts_bool parse_args(int argc, char **argv);



/** @brief Prints the position of vertices for the whole list
 *  
 *  The function is meant more or less as a debug tool, but can be used in production
 *  environment aswell.
 *  the output is in form of idx: x y z
 *  	@param *vlist is a structure holding information on vertex list.
 *	@returns TS_SUCCESS on successful execution, TS_FAIL otherwise.
*/

ts_bool print_vertex_list(ts_vertex_list *vlist);

/** @brief Prints the neighbours of all the vertices
 *  
 *  The function is meant more or less as a debug tool, but can be used in production
 *  environment aswell.
 *  the output is in form of idx(number of neighbours): (x1,y1,z1) (x2,y2,z2) ...
 *  	@param *vlist is a structure holding information on vertex list.
 *	@returns TS_SUCCESS on successful execution, TS_FAIL otherwise.
*/
ts_bool print_vertex_neighbours(ts_vertex_list *vlist);


/** @brief Function outputs the vetex list file to comply to old fortran format
 *  
 *  	@param *vlist is a list of vertices
 *  	@param *filename is a name of the output file to be created (note that if the file already
 *  exists it will be overwritten.
 */
ts_bool write_vertex_fcompat_file(ts_vertex_list *vlist,ts_char *filename);
ts_bool fprint_vertex_list(FILE *fh,ts_vertex_list *vlist);
ts_bool fprint_tristar(FILE *fh, ts_vesicle *vesicle);
ts_bool fprint_triangle_list(FILE *fh, ts_vesicle *vesicle);
ts_bool fprint_vertex_data(FILE *fh,ts_vertex_list *vlist);
ts_bool fprint_bonds(FILE *fh,ts_vesicle *vesicle);
ts_bool write_dout_fcompat_file(ts_vesicle *vesicle, ts_char *filename);
ts_bool read_tape_fcompat_file(ts_vesicle *vesicle, ts_char *filename);


/** @brief Outputs file in vtk format, compatible with paraview.
 *
 *	@param *vlist is a list of vertices
 *	@param *filename is a name of the output file. If exists, it will be overwritten
 *	@param *text is a description line (max. 255 characters) to be included in the file
 */
ts_bool write_vertex_vtk_file(ts_vesicle *vesicle,ts_char *filename, ts_char *text);
ts_bool write_vertex_xml_file(ts_vesicle *vesicle, ts_uint timestepno);
ts_bool write_master_xml_file(ts_char *filename);
ts_tape *parsetape(char *filename);
ts_bool tape_free(ts_tape *tape);

ts_bool dump_state(ts_vesicle *vesicle, ts_uint iteration);
ts_vesicle *restore_state(ts_uint *iteration);
#endif
