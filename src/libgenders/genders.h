/*
 * $Id: genders.h,v 1.1 2003-02-10 22:15:26 achu Exp $
 * $Source: /g/g0/achu/temp/genders-cvsbackup-full/genders/src/libgenders/genders.h,v $
 */

#ifndef _GENDERS_H
#define _GENDERS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>

/******************
 * defines        *
 ******************/

/* genders error codes */
#define GENDERS_ERR_SUCCESS     0 /* success */
#define GENDERS_ERR_OPEN        1 /* could not open file (errno valid) */
#define GENDERS_ERR_READ        2 /* could not read file (errno valid) */
#define GENDERS_ERR_PARSE       3 /* parse error on genders file */
#define GENDERS_ERR_NOTOPEN     4 /* genders file not open */
#define GENDERS_ERR_NOTCLOSED   5 /* genders file is not closed */
#define GENDERS_ERR_OVERFLOW    6 /* array is not big enough for result */
#define GENDERS_ERR_PARAMETERS  7 /* incorrect parameters passed in */
#define GENDERS_ERR_NOTFOUND    8 /* node not found */
#define GENDERS_ERR_OUTMEM      9 /* out of memory */
#define GENDERS_ERR_FREEMEM    10 /* memory has not been freed */
#define GENDERS_ERR_ISFREE     11 /* memory is already free */
#define GENDERS_ERR_INTERNAL   12 /* internal system error */

/* other definitions */
#define GENDERS_ERR_MIN         GENDERS_ERR_SUCCESS
#define GENDERS_ERR_MAX         GENDERS_ERR_INTERNAL
#define DEFAULT_GENDERS_FILE    "/etc/genders"

/* genders handle
 * errnum - error number returned to user
 * filename - genders filename
 * file - file stream for genders file
 * numnodes - number of nodes in the genders file
 * numattrs - number of attributes in the genders file
 * maxattrs - maximum number of attributes in the genders file for one node
 * maxxnodelen - maximum name length of a node
 * maxattrlen - maximum name length of an attribute
 * maxvallen - maximum name length of a value
 * allocatd - number of lists that have been allocated with *create functions
 * nodename - name of the current node
 * nodes_head - head of list that stores node names
 * nodes_tail - tail of list that stores node names
 * attrs_head - head of list that stores all unique attributes
 * attrs_tail - tail of list that stores all unique attributes
 * time_genders_loaded - time when the genders file was opened
 */

struct genders {
  int errnum;
  char *filename;
  FILE *file;
  int numnodes;
  int numattrs;
  int maxattrs;
  int maxnodelen;
  int maxattrlen;
  int maxvallen;
  int allocated_count;
  char nodename[MAXHOSTNAMELEN+1];
  void *nodes_head;
  void *nodes_tail;
  void *attrs_head;
  void *attrs_tail;
  time_t time_genders_loaded;
};

typedef struct genders *genders_t;

/******************
 * API            *
 ******************/

/* Creates & Initializes a genders handle 
 * Returns: NULL on memory allocation error
 */
genders_t genders_handle_create(void);

/* Destroys a genders handle.
 * Returns: 0 on success, -1 on failure.
 */
int genders_handle_destroy(genders_t handle);

/* Opens/reads/parses the specified genders file and associates it
 * with a handle.
 * If filename is NULL, attempts to read default genders file
 * Returns: 0 on success, -1 on failure.  */
int genders_open(genders_t handle, char *filename);

/* Closes the genders file associated with handle and frees cached
 * data.
 * Returns: 0 on success, -1 on failure.  */
int genders_close(genders_t handle);

/* Returns an error code associated with a handle 
 * Returns -1 if parameter is not a valid handle.
 */
int genders_errnum(genders_t handle);

/* Translates a genders error code to a text message.  
 * Returns: pointer to NULL-terminated statically allocated string.
 * Returns -1 if errnum out of range.
 */
char *genders_strerror(int errnum);

/* Produces a message on standard error describing the most recent
 * error that occurred.  
*/
void genders_perror(genders_t handle, char *msg);

/* Dumps the genders file stored in the handle to the file stream.
 * If stream is NULL, dumps to stdout.
 * Returns: 0 on success, -1 on failure */
int genders_handle_dump(genders_t handle, FILE *stream);

/* Get the number of nodes read from the genders file
 * Returns: number of nodes on success, -1 on failure 
 */
int genders_getnumnodes(genders_t handle);

/* Get the number of attributes read from the genders file
 * Returns: number of attributes on success, -1 on failure 
 */
int genders_getnumattrs(genders_t handle);

/* Get the max number of attributes read of any one node in the
 * genders file
 * Returns: number of attributes on success, -1 on failure */
int genders_getmaxattrs(genders_t handle);

/* Get the max node name length of any one node in the genders file
 * Returns: maximum node length on success, -1 on failure */
int genders_getmaxnodelen(genders_t handle);

/* Get the max attribute name length of any one node in the genders
 * file
 * Returns: maximum attribute length on success, -1 on failure */
int genders_getmaxattrlen(genders_t handle);

/* Get the max value length of any one attribute in the genders file
 * Returns: maximum value length on success, -1 on failure */
int genders_getmaxvallen(genders_t handle);

/* Allocate an array of character strings to store node names in.
 * Returns: number of elements the list can store on succcess,
 *          -1 on failure
 */
int genders_nodelist_create(genders_t handle, char ***nodelist);

/* Clears the data stored in a previously created node list
 * Returns: 0 on success, -1 on failure
 */
int genders_nodelist_clear(genders_t handle, char **nodelist);

/* Frees memory of a previously created node list
 * Returns: 0 on success, -1 on failure
 */
int genders_nodelist_destroy(genders_t handle, char **nodelist);

/* Allocate an array of character strings to store attribute names in.
 * Returns: number of elements the list can store on succcess,
 *          -1 on failure
 */
int genders_attrlist_create(genders_t handle, char ***attrlist);

/* Clears the data stored in a previously created attribute list
 * Returns: 0 on success, -1 on failure
 */
int genders_attrlist_clear(genders_t handle, char **attrlist);

/* Frees memory of a previously created attribute list
 * Returns: 0 on success, -1 on failure
 */
int genders_attrlist_destroy(genders_t handle, char **attrlist);

/* Allocate an array of character strings to store values in.
 * Returns: number of elements the list can store on succcess,
 *          -1 on failure
 */
int genders_vallist_create(genders_t handle, char ***vallist);

/* Clears the data stored in a previously created value list
 * Returns: 0 on success, -1 on failure
 */
int genders_vallist_clear(genders_t handle, char **vallist);

/* Frees memory of a previously created value list
 * Returns: 0 on success, -1 on failure
 */
int genders_vallist_destroy(genders_t handle, char **vallist);

/* Get the name of the current node (node name is the shortened
 * hostname).
 * Returns: 0 on success, -1 on failure 
 */
int genders_getnodename(genders_t handle, char *node, int len);

/* Gets list of nodes with the specified attribute.  
 * If attr is NULL, gets all nodes.  
 * If val is non-NULL, get only nodes with attributes=val.
 * Nodes are returned in genders file order.
 * Returns: number of matches on success, -1 on failure.
 */
int genders_getnodes(genders_t handle, char *nodes[], int len, char *attr, 
		     char *val);

/* Gets list of attributes for the specified node.  
 * If node is NULL, gets all attributes for the current node.  
 * If vals array is non-NULL, stores any attribute values in it.
 * Returns: number of matches on success, -1 on failure.
 */
int genders_getattr(genders_t handle, char *attrs[], char *vals[], int len, 
		    char *node);

/* Gets all attributes stored in the genders file
 * Returns: number of attributes on success, -1 on failure
 */
int genders_getattr_all(genders_t handle, char *attrs[], int len);

/* Tests whether node has attr.
 * If node is NULL, tests the current node.
 * If val is non-NULL, stores the attribute values in it.
 * Returns: 1=true , 0=false, -1=failure.
 */
int genders_testattr(genders_t handle, char *node, char *attr, char *val);

/* Tests whether node has attr and attr equals val.
 * If node is NULL, tests the current node.
 * Returns: 1=true , 0=false, -1=failure.
 */
int genders_testattrval(genders_t handle, char *node, char *attr, char *val);

/* Returns the time (in seconds) since genders file was opened
 * Returns: 0 on success, -1 on failure
 */
int genders_elapsedtime(genders_t handle);

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

#endif /* _GENDERS_H */