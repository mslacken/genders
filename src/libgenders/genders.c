/*
 * $Id: genders.c,v 1.41 2003-05-22 00:20:24 achu Exp $
 * $Source: /g/g0/achu/temp/genders-cvsbackup-full/genders/src/libgenders/genders.c,v $
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif                         
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "genders.h"

/******************
 * defines        *
 ******************/

#define GENDERS_ERR_MIN           GENDERS_ERR_SUCCESS
#define GENDERS_ERR_MAX           GENDERS_ERR_ERRNUMRANGE

#define GENDERS_DATA_LOADED       1
#define GENDERS_DATA_NOT_LOADED   0

#define GENDERS_MAGIC_NUM         0xdeadbeef

#define GENDERS_GETLINE_BUFLEN    65536

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

/* genders handle
 * magic - magic number, ensure proper pointer passed in
 * errnum - error number returned to user
 * loaded_flag - indicates if genders data has been loaded
 * numnodes - number of nodes in the genders file
 * numattrs - number of attributes in the genders file
 * maxattrs - maximum number of attributes for one node
 * maxxnodelen - maximum name length of a node
 * maxattrlen - maximum name length of an attribute
 * maxvallen - maximum name length of a value
 * nodename - name of the current node
 * nodes_head - head of list that stores node names
 * nodes_tail - tail of list that stores node names
 * attrs_head - head of list that stores all unique attributes
 * attrs_tail - tail of list that stores all unique attributes
 */

struct genders {
  int magic;
  int errnum;
  int loaded_flag;
  int numnodes;
  int numattrs;
  int maxattrs;
  int maxnodelen;
  int maxattrlen;
  int maxvallen;
  char nodename[MAXHOSTNAMELEN+1];
  struct node_listnode *nodes_head;
  struct node_listnode *nodes_tail;
  struct attr_listnode *attrs_head;
  struct attr_listnode *attrs_tail;
};

/* node_listnode
 * - stores name of a node in the genders file and all the node's
 *   attributes 
 * - attrvals_head = NULL and attrvals_tail = NULL if there are no attributes
 *   associated with this node
 */
struct node_listnode {
  char name[MAXHOSTNAMELEN+1];
  struct node_listnode *next;
  struct attrval_listnode *attrvals_head;
  struct attrval_listnode *attrvals_tail;
};

/* attrval_listnode
 * - stores an attribute name and its value
 * - val = NULL if there is no value
 */
struct attrval_listnode {
  char *name;
  char *val;
  struct attrval_listnode *next;
};

/* attr_listnode
 * - stores name of a unique attribute in the gender's file
 */
struct attr_listnode {
  char *name;
  struct attr_listnode *next;
};

/* error messages */
static char * errmsg[] = {
  "success",
  "genders handle is null",
  "open file error",
  "read file error",
  "genders file parse error",
  "genders data not loaded",
  "genders data already loaded",
  "array or string passed in not large enough to store result",
  "incorrect parameters passed in",
  "null pointer reached in list", 
  "node not found",
  "out of memory",
  "genders handle magic number incorrect, improper handle passed in",
  "unknown internal error"
  "error number out of range",
};

/*****************************************
 * Internal Function Declarations        *
 *****************************************/

/* common error checks for genders handle 
 * Returns -1 on error, 0 on success
 */
static int genders_handle_err_check(genders_t handle);

/* common error checks for non-loaded genders data 
 * Returns -1 on error, 0 on success
 */
static int genders_unloaded_handle_err_check(genders_t handle);

/* common error checks for loaded genders data 
 * Returns -1 on error, 0 on success
 */
static int genders_loaded_handle_err_check(genders_t handle);

/* Initialize handle variables */
static void genders_handle_initialize(genders_t handle);

/* genders_readline
 * - create buffer and read a line from fd
 * - return a buffer pointer in buf of length buflen
 * - buffer returned only if read > 0 chars
 * - truncation occurred if returns >= buflen
 * Returns numbers of bytes read on success, -1 on error
 */
static int genders_readline(genders_t handle, int fd, int buflen, char **buf);

/* genders_getline
 * - portable version of getline(3) for genders
 * - buffer returned only if read > 0 chars
 * - user is responsible for freeing memory
 * - Returns length of line read on success, 0 for EOF, -1 on error 
 */ 
static int genders_getline(genders_t handle, int fd, char **buf);

/* parse a line from the genders file 
 * - if debug_line > 0, only debug the line of data, do not
 *   store any genders data in the handle
 * If debug_line == 0, Returns -1 on error, 0 on success
 * If debug_line > 0, Returns -1 on error, number of parse errors
 * found for the line on success
 */
static int genders_parse_line(genders_t handle, 
                              char *line, 
                              int debug_line, 
                              FILE *stream);

/* insert a node into the node list 
 * Returns -1 on error, 0 on success
 */
static int genders_insert_node_listnode(genders_t handle, char *name);

/* insert an attribute (and value if value != NULL) into an
 * attribute-value list 
 * Returns -1 on error, 0 on success
 */
static int genders_insert_attrval_listnode(genders_t handle, 
                                           char *attribute,
                                           char *value);

/* place unique attribute names into the unique attribute list 
 * Returns -1 on error, 0 on success
 */
static int genders_insert_attr_listnode(genders_t handle, char *attr);

/* free all members of the node, attribute-value, and and attribute
 * lists 
 * Returns -1 on error, 0 on success
 */
static void genders_free_cache(genders_t handle);

/* creates lists 
 * Returns -1 on error, 'entries' on success
 */
static int genders_list_create(genders_t handle, 
                               char ***list, 
                               int entries, 
                               int entry_length);

/* clears lists 
 * Returns -1 on error, 0 on success
 */
static int genders_list_clear(genders_t handle, 
                              char **list, 
                              int entries, 
                              int entry_length);

/* destroys lists 
 * Returns -1 on error, 0 on success
 */
static int genders_list_destroy(genders_t handle, char **list, int entries);

/* determines if node is stored in the handle's nodelist
 * Returns 1 if it is stored, 0 if it is not.  
 * Pointer to node is stored in node_listnode
 */        
int genders_has_node(genders_t handle, 
                     const char *node, 
                     struct node_listnode **node_listnode);

/* determines if attr is an attribute for the node pointed to be
 * node_listnode.
 * Returns 1 if it is stored, 0 if it is not.  
 * Pointer to attribute is stored in attrval_listnode
 */        
int genders_has_attr(genders_t handle, 
                     const char *attr, 
                     struct node_listnode *node_listnode,
                     struct attrval_listnode **attrval_listnode);

int genders_handle_err_check(genders_t handle) {
  if (handle == NULL || handle->magic != GENDERS_MAGIC_NUM)
    return -1;

  return 0;
}

int genders_unloaded_handle_err_check(genders_t handle) {
  
  if (genders_handle_err_check(handle) == -1)
    return -1;

  if (handle->loaded_flag == GENDERS_DATA_LOADED) {
    handle->errnum = GENDERS_ERR_ISLOADED;
    return -1;
  }
  
  return 0;
}

int genders_loaded_handle_err_check(genders_t handle) {
  
  if (genders_handle_err_check(handle) == -1)
    return -1;

  if (handle->loaded_flag == GENDERS_DATA_NOT_LOADED) {
    handle->errnum = GENDERS_ERR_NOTLOADED;
    return -1;
  }
  
  return 0;
}

genders_t genders_handle_create(void) {
  genders_t handle;

  if ((handle = (genders_t)malloc(sizeof(struct genders))) == NULL)
    return NULL;
  
  genders_handle_initialize(handle);

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle;
}

void genders_handle_initialize(genders_t handle) {
  handle->magic = GENDERS_MAGIC_NUM;
  handle->loaded_flag = GENDERS_DATA_NOT_LOADED;
  handle->numnodes = 0;
  handle->numattrs = 0;
  handle->maxattrs = 0;
  handle->maxnodelen = 0;
  handle->maxattrlen = 0;
  handle->maxvallen = 0;
  memset(handle->nodename,'\0',MAXHOSTNAMELEN+1);
  handle->nodes_head = NULL;
  handle->nodes_tail = NULL;
  handle->attrs_head = NULL;
  handle->attrs_tail = NULL;
}

int genders_handle_destroy(genders_t handle) {

  if (genders_handle_err_check(handle) == -1)
    return -1;

  genders_free_cache(handle);

  genders_handle_initialize(handle);

  /* "clean" magic number 
   * ~0xdeadbeef == 0xlivebeef?
   */
  handle->magic = ~GENDERS_MAGIC_NUM;

  free(handle);
  return 0;
}

int genders_load_data(genders_t handle, const char *filename) {
  char *line = NULL;
  char *temp;
  int ret, fd = -1;

  if (genders_unloaded_handle_err_check(handle) == -1)
    return -1;

  if (filename == NULL)
    filename = GENDERS_DEFAULT_FILE;

  if ((fd = open(filename, O_RDONLY)) == -1) {
    handle->errnum = GENDERS_ERR_OPEN;
    goto cleanup;
  }

  /* parse genders file line by line */
  while ((ret = genders_getline(handle, fd, &line)) > 0) {
    if (genders_parse_line(handle, line, 0, NULL) == -1)
      goto cleanup;
    free(line);
  }
  line = NULL;

  if (ret == -1) 
    goto cleanup;

  if (gethostname(handle->nodename, MAXHOSTNAMELEN+1) == -1) {
    handle->errnum = GENDERS_ERR_INTERNAL;
    goto cleanup;
  }

  /* NUL-terminate nodename in case gethostname truncated */
  handle->nodename[MAXHOSTNAMELEN]='\0';

  /* shorten hostname if necessary */
  if ((temp = strchr(handle->nodename,'.')) != NULL)
    *temp = '\0';
  
  if (strlen(handle->nodename) > handle->maxnodelen) 
    handle->maxnodelen = strlen(handle->nodename);

  close(fd);

  /* data completely loaded, indicate in handle */
  handle->loaded_flag = GENDERS_DATA_LOADED;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;

cleanup:

  close(fd);
  free(line);
 
  genders_free_cache(handle);
  genders_handle_initialize(handle);

  return -1;
}

int genders_readline(genders_t handle, int fd, int buflen, char **buf) {
  int ret, count = 0;
  char *buffer = NULL;
  char chr;

  if ((buffer = (char *)malloc(buflen)) == NULL) {
    handle->errnum = GENDERS_ERR_OUTMEM;
    goto cleanup;
  }
  memset(buffer, '\0', buflen);
    
  /* read line */
  do {
    ret = read(fd, &chr, 1);
    if (ret == -1) {
      handle->errnum = GENDERS_ERR_READ;
      goto cleanup;
    }
    else if (ret == 1)
      buffer[count++] = chr;

  } while (ret == 1 && chr != '\n' && count < buflen);

  if (count == 0)
    free(buffer);
  else
    *buf = buffer;
  
  return count;

 cleanup:

  free(buffer);
  return -1;
}

int genders_getline(genders_t handle, int fd, char **buf) {
  int start_offset = 0;
  int retval, buflen = GENDERS_GETLINE_BUFLEN;
  
  /* get beginning seek position */
  if ((start_offset = lseek(fd, 0, SEEK_CUR)) == (off_t)-1) {
    handle->errnum = GENDERS_ERR_INTERNAL;
    return -1;
  }

  while ((retval = genders_readline(handle, fd, buflen, buf)) >= buflen) {
    if (lseek(fd, start_offset, SEEK_SET) != start_offset) {
      handle->errnum = GENDERS_ERR_INTERNAL;
      return -1;
    }
    buflen += GENDERS_GETLINE_BUFLEN;
    free(buf);
  }

  return retval;
}

int genders_parse_line(genders_t handle, 
                       char *line, 
                       int debug_line, 
                       FILE *stream) {
  char *attribute = NULL, *value = NULL;
  char *linebuf, *attrbuf;
  char *line_token = NULL;
  char *temp;
  int attrcount = 0;
  int ret;

  /* "remove" comments */
  if ((temp = strchr(line, '#')) != NULL) 
    *temp = '\0';

  /* move forward to node name */
  while(isspace(*line))  
    line++;

  /* get node name */
  if (*line != '\0') {
    line_token = strsep(&line, " \t\n\0");

    if (line_token != NULL) {

      if (strlen(line_token) > MAXHOSTNAMELEN) {
        if (debug_line > 0) {
          fprintf(stream, "Line %d: hostname too long\n", debug_line);
          return 1;
        }
        handle->errnum = GENDERS_ERR_PARSE;
        return -1;
      }

      if (debug_line == 0) {
        if (genders_insert_node_listnode(handle,line_token) == -1)
          return -1;

        handle->numnodes++;

        if (strlen(line_token) > handle->maxnodelen)
          handle->maxnodelen = strlen(line_token);
      }

      /* move forward to attributes */
      while(isspace(*line))  
        line++;
    }
  }
  else
    return 0;

  /* parse attributes */
  if (*line != '\0') {

    if (strchr(line,' ') != NULL || strchr(line,'\t') != NULL) {
      if (debug_line > 0) {
        fprintf(stream, "Line %d: white space in attribute list\n", 
                debug_line);
        return 1;
      }

      handle->errnum = GENDERS_ERR_PARSE;
      return -1;
    }

    line_token = strtok_r(line,",\n\0",&linebuf);
    while (line_token != NULL) {

      /* parse value out of attribute */
      if (strchr(line_token,'=') == NULL) {
        attribute = line_token;
        value = NULL;
      }
      else {
        attribute = strtok_r(line_token,"=",&attrbuf);
        value = strtok_r(NULL,"\0",&attrbuf);
      }

      if (debug_line == 0) {
        if (genders_insert_attrval_listnode(handle, 
                                            attribute,
                                            value) == -1)
          return -1;
        
        if (strlen(attribute) > handle->maxattrlen)
          handle->maxattrlen = strlen(attribute);
      
        if ((value != NULL) && (strlen(value) > handle->maxvallen))
          handle->maxvallen = strlen(value);

        if ((ret = genders_insert_attr_listnode(handle, attribute)) == -1)
          return -1;
      
        handle->numattrs += ret;

        attrcount++;
      }

      line_token = strtok_r(NULL,",\n\0",&linebuf);
    }
  }

  if (debug_line == 0 && attrcount > handle->maxattrs)
      handle->maxattrs = attrcount;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_insert_node_listnode(genders_t handle, char *name) {
  struct node_listnode *node_list;
  
  /* create new node at the end of the list */
  node_list = handle->nodes_tail;
  if (node_list == NULL) {
    handle->nodes_head = (void *)malloc(sizeof(struct node_listnode));
    if (handle->nodes_head == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    handle->nodes_tail = handle->nodes_head;
  }
  else {
    node_list->next = (void *)malloc(sizeof(struct node_listnode));
    if (node_list->next == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    handle->nodes_tail = node_list->next;
  }
  node_list = handle->nodes_tail;

  memset(node_list->name, '\0', MAXHOSTNAMELEN+1);

  /* previously asserted that name is < MAXHOSTNAMELEN+1 */
  strcpy(node_list->name, name);
  node_list->next = NULL;
  node_list->attrvals_head = NULL;
  node_list->attrvals_tail = NULL;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_insert_attrval_listnode(genders_t handle, 
                                    char *attribute,
                                    char *value) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;

  node_list = handle->nodes_tail;
  attrval_list = node_list->attrvals_tail;

  /* create new node at the end of the list */
  if (attrval_list == NULL) {
    node_list->attrvals_head = (void *)malloc(sizeof(struct attrval_listnode));

    if (node_list->attrvals_head == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    node_list->attrvals_tail = node_list->attrvals_head;
  }
  else {
    attrval_list->next = (void *)malloc(sizeof(struct attrval_listnode));
    if (attrval_list->next == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    node_list->attrvals_tail = attrval_list->next;
  }
  attrval_list = node_list->attrvals_tail;

  /* store attribute */
  attrval_list->name = (char *)malloc(strlen(attribute)+1);
  if (attrval_list->name == NULL) {
    handle->errnum = GENDERS_ERR_OUTMEM;
    return -1;
  }
  strcpy(attrval_list->name,attribute);

  /* store value */
  if (value != NULL) {
    attrval_list->val = (char *)malloc(strlen(value)+1);
    if (attrval_list->val == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    strcpy(attrval_list->val,value);
  }
  else 
    attrval_list->val = NULL; 

  attrval_list->next = NULL;
 
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_insert_attr_listnode(genders_t handle, char *attr) {
  struct attr_listnode *attr_list;

  /* is attribute already in the list? */
  attr_list = handle->attrs_head;
  while (attr_list != NULL && strcmp(attr_list->name, attr) != 0)
    attr_list = attr_list->next;

  if (attr_list != NULL)
    return 0;

  /* create new node at the end of the list */
  attr_list = handle->attrs_tail;
  if (attr_list == NULL) {
    handle->attrs_head = (void *)malloc(sizeof(struct attr_listnode));
    if (handle->attrs_head == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    handle->attrs_tail = handle->attrs_head;
  }
  else {
    attr_list->next = (void *)malloc(sizeof(struct attr_listnode));
    if (attr_list->next == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    handle->attrs_tail = attr_list->next;
  }
  attr_list = handle->attrs_tail;

  /* store attribute */
  attr_list->name = (char *)malloc(strlen(attr) + 1);
  if (attr_list->name == NULL) {
    handle->errnum = GENDERS_ERR_OUTMEM;
    return -1;
  }
  strcpy(attr_list->name,attr);
  attr_list->next = NULL;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 1;
}

void genders_free_cache(genders_t handle) {
  struct node_listnode *node_list;
  struct attr_listnode *attr_list;
  
  /* free node list and each node's attributes */
  node_list = handle->nodes_head;
  while (node_list != NULL) {
    struct node_listnode *temp_node_list = node_list->next;
    struct attrval_listnode *attrval_list = node_list->attrvals_head;
 
    while (attrval_list != NULL) {
      struct attrval_listnode * temp_attrval_list = attrval_list->next;
      free(attrval_list->name);
      free(attrval_list->val);
      free(attrval_list);
      attrval_list = temp_attrval_list;
    }
    
    free(node_list);
    node_list = temp_node_list;
  }

  /* free unique attribute list */
  attr_list = handle->attrs_head;
  while (attr_list != NULL) {
    struct attr_listnode *temp_attr_list = attr_list->next;
    free(attr_list->name);
    free(attr_list);
    attr_list = temp_attr_list;
  }
}

int genders_errnum(genders_t handle) {
  if (handle == NULL)
    return GENDERS_ERR_NULLHANDLE;
  else if (handle->magic != GENDERS_MAGIC_NUM)
    return GENDERS_ERR_MAGIC;
  else
    return handle->errnum;
}

char *genders_strerror(int errnum) {
  if (errnum >= GENDERS_ERR_MIN && errnum <= GENDERS_ERR_MAX)
    return errmsg[errnum];
  else
    return errmsg[GENDERS_ERR_ERRNUMRANGE];
}

char *genders_errormsg(genders_t handle) {
  return genders_strerror(genders_errnum(handle));
}

void genders_perror(genders_t handle, const char *msg) {
  char *errormsg;

  errormsg = genders_strerror(genders_errnum(handle));

  if (msg == NULL)
    fprintf(stderr, "%s\n", errormsg);
  else
    fprintf(stderr, "%s: %s\n", msg, errormsg);
}

int genders_handle_dump(genders_t handle, FILE *stream) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (stream == NULL)
    stream = stdout;

  node_list = handle->nodes_head;
  while (node_list != NULL) {
    fprintf(stream,"%s\t", node_list->name);

    attrval_list = node_list->attrvals_head;
    while (attrval_list != NULL) {
      if (attrval_list->val != NULL)
        fprintf(stream,"%s=%s",attrval_list->name,attrval_list->val);
      else 
        fprintf(stream,"%s", attrval_list->name);

      if (attrval_list->next != NULL)
        fprintf(stream,",");

      attrval_list = attrval_list->next;
    }

    fprintf(stream,"\n");
    node_list = node_list->next;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_getnumnodes(genders_t handle) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numnodes;
}

int genders_getnumattrs(genders_t handle) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numattrs;
}

int genders_getmaxattrs(genders_t handle) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxattrs;
}

int genders_getmaxnodelen(genders_t handle) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxnodelen;
}

int genders_getmaxattrlen(genders_t handle) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;


  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxattrlen;
}

int genders_getmaxvallen(genders_t handle) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxvallen;
}

int genders_nodelist_create(genders_t handle, char ***nodelist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_create(handle, 
                             nodelist, 
                             handle->numnodes, 
                             handle->maxnodelen + 1);
}

int genders_nodelist_clear(genders_t handle, char **nodelist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_clear(handle, 
                            nodelist, 
                            handle->numnodes, 
                            handle->maxnodelen + 1);
}

int genders_nodelist_destroy(genders_t handle, char **nodelist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_destroy(handle, nodelist, handle->numnodes);
}

int genders_attrlist_create(genders_t handle, char ***attrlist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_create(handle, 
                             attrlist, 
                             handle->numattrs, 
                             handle->maxattrlen + 1);
}

int genders_attrlist_clear(genders_t handle, char **attrlist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_clear(handle, 
                            attrlist, 
                            handle->numattrs, 
                            handle->maxattrlen + 1);
}

int genders_attrlist_destroy(genders_t handle, char **attrlist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;
  
  return genders_list_destroy(handle, attrlist, handle->numattrs);
}

int genders_vallist_create(genders_t handle, char ***vallist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_create(handle, 
                             vallist, 
                             handle->numattrs, 
                             handle->maxvallen + 1);
}

int genders_vallist_clear(genders_t handle, char **vallist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_clear(handle, 
                            vallist, 
                            handle->numattrs, 
                            handle->maxvallen + 1);
}

int genders_vallist_destroy(genders_t handle, char **vallist) {

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  return genders_list_destroy(handle, vallist, handle->numattrs);
}

int genders_list_create(genders_t handle, 
                        char ***list, 
                        int entries, 
                        int entry_length) {
  char **temp;
  int i,j;

  if (list == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (entries > 0) {
    temp = (char **)malloc(sizeof(char *) * entries);
    if (temp == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }

    for (i = 0; i < entries; i++) {
      temp[i] = (char *)malloc(entry_length);
      if (temp[i] == NULL) {
        
        for (j = 0; j < i; j++)
          free(temp[j]);
        free(temp);

        handle->errnum = GENDERS_ERR_OUTMEM;
        return -1;
      }
      memset(temp[i], '\0', entry_length);
    }

    *list = temp;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return entries;
}

int genders_list_clear(genders_t handle,
                       char **list, 
                       int entries, 
                       int entry_length) {
  int i;

  if (list == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  for (i = 0; i < entries; i++) {
    if (list[i] == NULL) {
      handle->errnum = GENDERS_ERR_NULLPTR;
      return -1;
    }
    memset(list[i], '\0', entry_length);
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_list_destroy(genders_t handle,
                         char **list, 
                         int entries) {
  int i;

  if (list == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (entries > 0) {
    for (i = 0; i < entries; i++)
      free(list[i]);
    free(list);
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_getnodename(genders_t handle, char *node, int len) {
  
  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (node == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if ((strlen(handle->nodename) + 1) > len) {
    handle->errnum = GENDERS_ERR_OVERFLOW;
    return -1;
  }

  strcpy(node, handle->nodename);
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_getnodes(genders_t handle, 
                     char *nodes[], 
                     int len, 
                     const char *attr, 
                     const char *val) {
  
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_listnode;
  int nodecount = 0;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (nodes == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  /* search nodes */
  node_list = handle->nodes_head;
  while (node_list != NULL) {
   
    if (attr != NULL) {

      if (genders_has_attr(handle, attr, node_list, &attrval_listnode) == 1) {

        if ((val == NULL) || (val != NULL && 
                              attrval_listnode->val != NULL &&
                              strcmp(attrval_listnode->val, val) == 0)) {

          if (nodecount >= len) {
            handle->errnum = GENDERS_ERR_OVERFLOW;
            return -1;
          }

          if (nodes[nodecount] == NULL) {
            handle->errnum = GENDERS_ERR_NULLPTR;
            return -1;
          }

          strcpy(nodes[nodecount++], node_list->name); 
        }
      }
    }
    else {
      if (nodecount >= len) {
        handle->errnum = GENDERS_ERR_OVERFLOW;
        return -1;
      }
      strcpy(nodes[nodecount++], node_list->name); 
    }

    node_list = node_list->next;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return nodecount;
}

int genders_getattr(genders_t handle, 
                    char *attrs[], 
                    char *vals[], 
                    int len, 
                    const char *node) {

  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;
  int attrcount = 0;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (attrs == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (node == NULL)
    node = handle->nodename;

  if (genders_has_node(handle, node, &node_list) == 0) {
    handle->errnum = GENDERS_ERR_NOTFOUND;
    return -1;
  }

  /* store attributes */
  attrval_list = node_list->attrvals_head;
  while (attrval_list != NULL) {
    if (attrcount >= len) {
      handle->errnum = GENDERS_ERR_OVERFLOW;
      return -1;
    }    

    if (attrs[attrcount] == NULL) {
      handle->errnum = GENDERS_ERR_NULLPTR;
      return -1;
    }
    strcpy(attrs[attrcount],attrval_list->name);

    if (vals != NULL && attrval_list->val != NULL) {
      if (vals[attrcount] == NULL) {
        handle->errnum = GENDERS_ERR_NULLPTR;
        return -1;
      }
      strcpy(vals[attrcount],attrval_list->val);
    }
    attrcount++;

    attrval_list = attrval_list->next;
  }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return attrcount;  
}

int genders_getattr_all(genders_t handle, char *attrs[], int len) {
  struct attr_listnode *attr_list;
  int attrcount = 0;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (attrs == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (handle->numattrs > len) {
    handle->errnum = GENDERS_ERR_OVERFLOW;
    return -1;
  }

  /* store attributes */
  attr_list = handle->attrs_head;
  while (attr_list != NULL) {
    if (attrs[attrcount] == NULL) {
      handle->errnum = GENDERS_ERR_NULLPTR;
      return -1;
    }
    strcpy(attrs[attrcount++],attr_list->name);
    attr_list = attr_list->next;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return attrcount;  
}

int genders_testattr(genders_t handle, 
                     const char *node, 
                     const char *attr, 
                     char *val, 
                     int len) {
  struct node_listnode *node_listnode;
  struct attrval_listnode *attrval_listnode;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (attr == NULL || (val != NULL && len <= 0)) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (node == NULL)
    node = handle->nodename;

  if (genders_has_node(handle, node, &node_listnode) == 0) {
    handle->errnum = GENDERS_ERR_NOTFOUND;
    return -1;
  }

  if (genders_has_attr(handle, attr, node_listnode, &attrval_listnode) == 1) {
    if (val != NULL && attrval_listnode->val != NULL) {
      if ((strlen(attrval_listnode->val) + 1) > len) {
        handle->errnum = GENDERS_ERR_OVERFLOW;
        return -1;
      }
      strcpy(val, attrval_listnode->val);
    }
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 1;  
  }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_testattrval(genders_t handle, 
                        const char *node, 
                        const char *attr, 
                        const char *val) {
  struct node_listnode *node_listnode;
  struct attrval_listnode *attrval_listnode;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (attr == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (node == NULL)
    node = handle->nodename;

  if (genders_has_node(handle, node, &node_listnode) == 0) {
    handle->errnum = GENDERS_ERR_NOTFOUND;
    return -1;
  }

  if (genders_has_attr(handle, attr, node_listnode, &attrval_listnode) == 1) {

    if ((val == NULL) || ((val != NULL) && 
                          (attrval_listnode->val != NULL) && 
                          (strcmp(attrval_listnode->val, val) == 0))) {

      handle->errnum = GENDERS_ERR_SUCCESS;
      return 1;  
    }
  }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_isnode(genders_t handle, const char *node) {
  int retval;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (node == NULL)
    node = handle->nodename;

  retval = genders_has_node(handle, node, NULL);

  handle->errnum = GENDERS_ERR_SUCCESS;
  return retval;
}

int genders_isattr(genders_t handle, const char *attr) {
  int retval = 0;
  struct attr_listnode *attr_list;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (attr == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  attr_list = handle->attrs_head;
  while (attr_list != NULL && strcmp(attr_list->name,attr) != 0)
    attr_list = attr_list->next;

  if (attr_list != NULL)
    retval = 1;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return retval;
}

int genders_isattrval(genders_t handle, const char *attr, const char *val) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;

  if (genders_loaded_handle_err_check(handle) == -1)
    return -1;

  if (attr == NULL || val == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }
  
  node_list = handle->nodes_head;
  while (node_list != NULL) {
    attrval_list = node_list->attrvals_head;
    
    if (genders_has_attr(handle, attr, node_list, &attrval_list) == 1) {
      if (attrval_list->val != NULL && strcmp(attrval_list->val, val) == 0) {
        handle->errnum = GENDERS_ERR_SUCCESS;
        return 1;
      }
    }
    
    node_list = node_list->next;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_has_node(genders_t handle, 
                     const char *node, 
                     struct node_listnode **node_listnode) {
  struct node_listnode *node_list;

  node_list = handle->nodes_head;
  while (node_list != NULL && strcmp(node_list->name,node) != 0)
    node_list = node_list->next;

  if (node_listnode != NULL)
    *node_listnode = node_list;

  if (node_list != NULL)
    return 1;

  return 0;
}

int genders_has_attr(genders_t handle, 
                     const char *attr, 
                     struct node_listnode *node_listnode,
                     struct attrval_listnode **attrval_listnode) {

  struct attrval_listnode *attrval_list;

  attrval_list = node_listnode->attrvals_head;
  while (attrval_list != NULL && strcmp(attrval_list->name,attr) != 0)
    attrval_list = attrval_list->next;
  
  if (attrval_list != NULL) {
    if (attrval_listnode != NULL)
      *attrval_listnode = attrval_list;
    return 1;
  }

  return 0;
}

int genders_parse(genders_t handle, const char *filename, FILE *stream) {
  int ret, line_count = 1, retval = 0, fd = -1;
  char *line = NULL;

  if (genders_handle_err_check(handle) == -1)
    return -1;

  if (filename == NULL)
    filename = GENDERS_DEFAULT_FILE;
  
  if (stream == NULL)
    stream = stderr;

  if ((fd = open(filename,O_RDONLY)) == -1) {
    handle->errnum = GENDERS_ERR_OPEN;
    goto cleanup;
  }

  while (genders_getline(handle, fd, &line) != 0) {
    if ((ret = genders_parse_line(handle, line, line_count, stream)) == -1)
      goto cleanup;

    retval += ret;
    free(line);
    line_count++;
  }
  line = NULL;

  close(fd);
  return retval;

 cleanup:

  free(line);
  close(fd);

  return -1;
}

void genders_set_errnum(genders_t handle, int errnum) {

  if (genders_handle_err_check(handle) == -1)
    return;

  if (errnum >= GENDERS_ERR_MIN && errnum <= GENDERS_ERR_MAX) 
    handle->errnum = errnum;
  else
    handle->errnum = GENDERS_ERR_INTERNAL;
}
