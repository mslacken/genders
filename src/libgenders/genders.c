/*
 * $Id: genders.c,v 1.1 2003-02-10 22:15:26 achu Exp $
 * $Source: /g/g0/achu/temp/genders-cvsbackup-full/genders/src/libgenders/genders.c,v $
 */

#include "genders.h"

/* node_listnode
 * - stores name of a node in the genders file and all the node's
 *   attributes 
 * - attrvals_head = NULL and attrvals_tail = NULL if there are no attributes
 *   associated with this node
 */
struct node_listnode {
  char *name;
  struct node_listnode *next;
  void *attrvals_head;
  void *attrvals_tail;
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
 * - stores name of an attributes in the gender's file
 */
struct attr_listnode {
  char *name;
  struct attr_listnode *next;
};

/* error messages */
static char * errmsg[] = {
  "success",
  "open file error",
  "read file error",
  "genders file parse error",
  "genders file not open",
  "genders file already open",
  "array passed in not large enough to store result",
  "incorrect parameters passed in",
  "node not found",
  "out of memory",
  "previously created lists not destroyed",
  "all previously created lists already destroyed",
  "unknown internal error"
};

/*****************************************
 * Internal Function Declarations        *
 *****************************************/

/* Initialize handle variables */
static void genders_handle_initialize(genders_t handle);

/* parse a line from the genders file */
static int genders_parse_line(genders_t handle, char *line);

/* insert a node into the node list */
static int genders_insert_node_listnode(genders_t handle, char *name);

/* insert an attribute (and value if necessary) into an attribute list */
static int genders_insert_attrval_listnode(genders_t handle, char *name);

/* place unique attribute names into the unique attribute list */
static int genders_insert_attr_listnode(genders_t handle, char *attr);

/* free all members of the cache (i.e. node and attribute lists) */
static void genders_free_cache(genders_t handle);

/* creates lists */
static int genders_list_create(genders_t handle, char ***list, int entries, 
			       int entry_length);

/* clears lists */
static int genders_list_clear(genders_t handle, char **list, int entries, 
			      int entry_length);

/* destroys lists */
static int genders_list_destroy(genders_t handle, char **list, int entries);

genders_t genders_handle_create(void) {
  genders_t handle;

  handle = (genders_t)malloc(sizeof(struct genders));
  if (handle == NULL) {
    return NULL;
  }
  
  genders_handle_initialize(handle);

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle;
}

void genders_handle_initialize(genders_t handle) {
  handle->filename = NULL;
  handle->file = NULL;
  handle->numnodes = 0;
  handle->numattrs = 0;
  handle->maxattrs = 0;
  handle->maxnodelen = 0;
  handle->maxattrlen = 0;
  handle->maxvallen = 0;
  handle->allocated_count = 0;
  memset(handle->nodename,'\0',MAXHOSTNAMELEN+1);
  handle->nodes_head = NULL;
  handle->nodes_tail = NULL;
  handle->attrs_head = NULL;
  handle->attrs_tail = NULL;
  handle->time_genders_loaded = 0;
}

int genders_handle_destroy(genders_t handle) {

  if (handle == NULL) {
    return -1;
  }

  if (handle->file != NULL) {
    handle->errnum = GENDERS_ERR_NOTCLOSED;
    return -1;
  }

  free(handle);
  return 0;
}

int genders_open(genders_t handle, char *filename) {
  char *line = NULL;
  char *temp;
  int len;

  if (handle == NULL) {
    return -1;
  }

  if (handle->file != NULL) {
    handle->errnum = GENDERS_ERR_NOTCLOSED;
    return -1;
  }

  if (filename == NULL) {
    filename = DEFAULT_GENDERS_FILE;
  }

  handle->filename = (char *)malloc(strlen(filename)+1);
  if (handle->filename == NULL) {
    handle->errnum = GENDERS_ERR_OUTMEM;
    goto cleanup;
  }
  strcpy(handle->filename,filename);

  if ((handle->file = fopen(handle->filename,"r")) == NULL) {
    handle->errnum = GENDERS_ERR_OPEN;
    goto cleanup;
  }

  while (getline(&line, &len, handle->file) != -1) {
    if (genders_parse_line(handle, line) == -1) {
      goto cleanup;
    }
  }
  
  if (line != NULL) {
    free(line);
  }

  if ((handle->time_genders_loaded = time(NULL)) == (time_t)-1) {
    handle->errnum = GENDERS_ERR_INTERNAL;
    goto cleanup;
  }

  if (gethostname(handle->nodename, MAXHOSTNAMELEN+1) == -1) {
    handle->errnum = GENDERS_ERR_INTERNAL;
    goto cleanup;
  }

  /* shorten hostname if necessary */
  if ((temp = strchr(handle->nodename,'.')) != NULL) {
    *temp = '\0';
  }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;

cleanup:

  if (line != NULL) {
    free(line);
  }

  if (handle->filename != NULL) {
    free(handle->filename);
  }
    
  if (handle->file != NULL) {
    fclose(handle->file);
  }
  
  genders_free_cache(handle);

  genders_handle_initialize(handle);

  return -1;
}

int genders_parse_line(genders_t handle, char *line) {
  char *token = NULL;
  char *temp;
  char *buf;
  int attrcount = 0;

  /* "remove" comments */
  if ((temp = strchr(line, '#')) != NULL) {
    *temp = '\0';
  }

  /* move forward to node name */
  while(*line == ' ' || *line == '\t' || *line == '\n' ) {  
    line++;
  }

  /* get node name */
  if (*line != '\0') {
    token = strsep(&line, " \t\n\0");
    if (token != NULL) {

      if (strlen(token) > MAXHOSTNAMELEN) {
	handle->errnum = GENDERS_ERR_PARSE;
	return -1;
      }

      if (genders_insert_node_listnode(handle,token) == -1) {
	return -1;
      }
      handle->numnodes++;

      if (strlen(token) > handle->maxnodelen) {
	handle->maxnodelen = strlen(token);
      }

      /* move forward to attributes */
      while(*line == ' ' || *line == '\t' || *line == '\n' ) {  
	line++;
      }
    }
  }

  /* get attributes */
  if (*line != '\0') {
    if (strspn(line," \t") > 0) {
      handle->errnum = GENDERS_ERR_PARSE;
      return -1;
    }

    /* Here, the parsing does not distinguish between attributes with
     * values or without values.  If the attribute has a value, it
     * will be parsed in genders_insert_attrval_listnode().  */
    token = strtok_r(line,",\n\0",&buf);
    while (token != NULL) {
      if (genders_insert_attrval_listnode(handle, token) == -1) {
	return -1;
      }
      attrcount++;
      token = strtok_r(NULL,",\n\0",&buf);
    }
    
  }

  if (attrcount > handle->maxattrs) {
    handle->maxattrs = attrcount;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_insert_node_listnode(genders_t handle, char *name) {
  struct node_listnode *node_list;
  
  node_list = (struct node_listnode *)handle->nodes_tail;
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
  node_list = (struct node_listnode *)handle->nodes_tail;

  node_list->name = (char *)malloc(strlen(name) + 1);
  if (node_list->name == NULL) {
    handle->errnum = GENDERS_ERR_OUTMEM;
    return -1;
  }
  strcpy(node_list->name,name);
  node_list->next = NULL;
  node_list->attrvals_head = NULL;
  node_list->attrvals_tail = NULL;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_insert_attrval_listnode(genders_t handle, char *attr) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;
  char *token;
  char *buf;

  node_list = (struct node_listnode *)handle->nodes_tail;
  attrval_list = (struct attrval_listnode *)node_list->attrvals_tail;

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
  attrval_list = (struct attrval_listnode *)node_list->attrvals_tail;

  /* handle attribute differently if it has a value, or if it
   * does not have a value 
   */
  if (strchr(attr,'=') == NULL) {
    attrval_list->name = (char *)malloc(strlen(attr)+1);
    if (attrval_list->name == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    strcpy(attrval_list->name,attr);
    attrval_list->val = NULL;

    if (genders_insert_attr_listnode(handle, attr) == -1) {
      return -1;
    }
  }
  else {

    if (strspn(attr,"=") > 1) {
      handle->errnum = GENDERS_ERR_PARSE;
      return -1;
    }

    /* parse attribute & value */
    token = strtok_r(attr,"=",&buf);
    attrval_list->name = (char *)malloc(strlen(token)+1);
    if (attrval_list->name == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    strcpy(attrval_list->name,token);

    if (genders_insert_attr_listnode(handle, attr) == -1) {
      return -1;
    }

    token = strtok_r(NULL,"\0",&buf);
    attrval_list->val = (char *)malloc(strlen(token)+1);
    if (attrval_list->val == NULL) {
      handle->errnum = GENDERS_ERR_OUTMEM;
      return -1;
    }
    strcpy(attrval_list->val,token);
  }
  attrval_list->next = NULL;
 
  if (strlen(attrval_list->name) > handle->maxattrlen) {
    handle->maxattrlen = strlen(attrval_list->name);
  }
  
  if ((attrval_list->val != NULL) && 
      (strlen(attrval_list->val) > handle->maxvallen)) {
    handle->maxvallen = strlen(attrval_list->val);
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_insert_attr_listnode(genders_t handle, char *attr) {
  struct attr_listnode *attr_list;

  /* is attribute already in the list? */
  attr_list = (struct attr_listnode *)handle->attrs_head;
  while (attr_list != NULL) {
    if (strcmp(attr_list->name, attr) == 0)
      return 0;
    attr_list = attr_list->next;
  }

  attr_list = (struct attr_listnode *)handle->attrs_tail;
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
  attr_list = (struct attr_listnode *)handle->attrs_tail;

  attr_list->name = (char *)malloc(strlen(attr) + 1);
  if (attr_list->name == NULL) {
    handle->errnum = GENDERS_ERR_OUTMEM;
    return -1;
  }
  strcpy(attr_list->name,attr);
  attr_list->next = NULL;

  handle->numattrs++;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

void genders_free_cache(genders_t handle) {
  struct node_listnode *node_list;
  struct node_listnode *temp_node_list;
  struct attrval_listnode *attrval_list;
  struct attrval_listnode *temp_attrval_list;
  struct attr_listnode *attr_list;
  struct attr_listnode *temp_attr_list;

  /* free node list and each node's attributes */
  node_list = (struct node_listnode *)handle->nodes_head;
  while (node_list != NULL) {
    temp_node_list = node_list->next;
    attrval_list = (struct attrval_listnode *)node_list->attrvals_head;
 
    while (attrval_list != NULL) {
      temp_attrval_list = attrval_list->next;
      free(attrval_list->name);
      free(attrval_list->val);
      free(attrval_list);
      attrval_list = temp_attrval_list;
    }
    
    free(node_list->name);
    free(node_list);
    node_list = temp_node_list;
  }

  /* free attribute list */
  attr_list = (struct attr_listnode *)handle->attrs_head;
  while (attr_list != NULL) {
    temp_attr_list = attr_list->next;
    free(attr_list->name);
    free(attr_list);
    attr_list = temp_attr_list;
  }
}

int genders_close(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }
  
  if (handle->allocated_count != 0) {
    handle->errnum = GENDERS_ERR_FREEMEM;
    return -1;
  }

  fclose(handle->file);
 
  free(handle->filename);

  genders_free_cache(handle);

  genders_handle_initialize(handle);

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_errnum(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  else {
    return handle->errnum;
  }
}

char *genders_strerror(int errnum) {
  if (errnum >= GENDERS_ERR_MIN && errnum <= GENDERS_ERR_MAX)
    return errmsg[errnum];
  else
    return NULL;
}

void genders_perror(genders_t handle, char *msg) {
  char *errormsg;

  if (handle == NULL)
    return;

  errormsg = genders_strerror(genders_errnum(handle));

  if (msg == NULL) {
    fprintf(stderr, "%s\n", errormsg);
  }
  else {
    fprintf(stderr, "%s: %s\n", msg, errormsg);
  }
}

int genders_handle_dump(genders_t handle, FILE *stream) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;

  if (handle == NULL) {
    return -1;
  }
 
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (stream == NULL) {
    stream = stdout;
  }

  node_list = handle->nodes_head;
  while (node_list != NULL) {
    fprintf(stream,"%s\t", node_list->name);

    attrval_list = node_list->attrvals_head;
    while (attrval_list != NULL) {
      if (attrval_list->val != NULL) {
	fprintf(stream,"%s=%s",attrval_list->name,attrval_list->val);
      }
      else {
	fprintf(stream,"%s", attrval_list->name);
      }
      if (attrval_list->next != NULL) {
	fprintf(stream,",");
      }
      attrval_list = attrval_list->next;
    }

    fprintf(stream,"\n");
    node_list = node_list->next;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_getnumnodes(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numnodes;
}

int genders_getnumattrs(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numattrs;
}

int genders_getmaxattrs(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxattrs;
}

int genders_getmaxnodelen(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxnodelen;
}

int genders_getmaxattrlen(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxattrlen;
}

int genders_getmaxvallen(genders_t handle) {
  if (handle == NULL) {
    return -1;
  }
  
  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->maxvallen;
}

int genders_nodelist_create(genders_t handle, char ***nodelist) {

  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numnodes == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (genders_list_create(handle, nodelist, handle->numnodes, 
			  handle->maxnodelen + 1) == -1) {
    return -1;
  }
  handle->allocated_count++;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numnodes;
}

int genders_nodelist_clear(genders_t handle, char **nodelist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numnodes == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (genders_list_clear(handle, nodelist, handle->numnodes, 
			 handle->maxnodelen + 1) == -1) {
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_nodelist_destroy(genders_t handle, char **nodelist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numnodes == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }
  
  if (handle->allocated_count == 0) {
    handle->errnum = GENDERS_ERR_ISFREE;
    return -1;
  }

  if (genders_list_destroy(handle, nodelist, handle->numnodes) == -1) {
    return -1;
  }
  handle->allocated_count--;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_attrlist_create(genders_t handle, char ***attrlist) {

  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numattrs == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (genders_list_create(handle, attrlist, handle->numattrs, 
			  handle->maxattrlen + 1) == -1) {
    return -1;
  }
  handle->allocated_count++;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numattrs;
}

int genders_attrlist_clear(genders_t handle, char **attrlist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numattrs == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (genders_list_clear(handle, attrlist, handle->numattrs, 
			 handle->maxattrlen + 1) == -1) {
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_attrlist_destroy(genders_t handle, char **attrlist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numattrs == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (handle->allocated_count == 0) {
    handle->errnum = GENDERS_ERR_ISFREE;
    return -1;
  }

  if (genders_list_destroy(handle, attrlist, handle->numattrs) == -1) {
    return -1;
  }
  handle->allocated_count--;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;  
}

int genders_vallist_create(genders_t handle, char ***vallist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numattrs == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (genders_list_create(handle, vallist, handle->numattrs, 
			  handle->maxvallen + 1) == -1) {
    return -1;
  }
  handle->allocated_count++;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return handle->numattrs;
}

int genders_vallist_clear(genders_t handle, char **vallist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numattrs == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (genders_list_clear(handle, vallist, handle->numattrs, 
			 handle->maxvallen + 1) == -1) {
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_vallist_destroy(genders_t handle, char **vallist) {
  if (handle == NULL) {
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }

  if (handle->numattrs == 0) {
    handle->errnum = GENDERS_ERR_SUCCESS;
    return 0;
  }

  if (handle->allocated_count == 0) {
    handle->errnum = GENDERS_ERR_ISFREE;
    return -1;
  }

  if (genders_list_destroy(handle, vallist, handle->numattrs) == -1) {
    return -1;
  }
  handle->allocated_count--;

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;  
}

int genders_list_create(genders_t handle, 
			char ***list, int entries, int entry_length) {
  char **temp;
  int i,j;

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

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_list_clear(genders_t handle,
		       char **list, int entries, int entry_length) {
  int i;

  for (i = 0; i < entries; i++) {
    memset(list[i], '\0', entry_length);
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_list_destroy(genders_t handle,
			 char **list, int entries) {
  int i;

  for (i = 0; i < entries; i++) {
    free(list[i]);
  }
  free(list);

  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_getnodename(genders_t handle, char *node, int len) {
  
  if (handle == NULL) {
    return -1;
  }

  if (node == NULL || len <= 0 || len > MAXHOSTNAMELEN+1) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  if (handle->file == NULL) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
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

int genders_getnodes(genders_t handle, char *nodes[], int len, char *attr, 
		     char *val) {
  
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;
  int nodecount = 0;

  if (handle == NULL) {
    return -1;
  }

  if (nodes == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }

  node_list = (struct node_listnode *)handle->nodes_head;
  while (node_list != NULL) {
   
    if (attr != NULL) {

      /* find attribute */
      attrval_list = (struct attrval_listnode *)node_list->attrvals_head;
      while (attrval_list != NULL) {
	if ((strcmp(attrval_list->name,attr) == 0) && 
	    ((val == NULL) || 
	     (val != NULL && attrval_list->val != NULL &&
	      strcmp(attrval_list->val,val) == 0))) {
	  if (nodecount >= len) {
	    handle->errnum = GENDERS_ERR_OVERFLOW;
	    return -1;
	  }
	  strcpy(nodes[nodecount], node_list->name); 
	  nodecount++;
	  break;
	}
	attrval_list = attrval_list->next;
      }
    }
    else {
      if (nodecount >= len) {
	handle->errnum = GENDERS_ERR_OVERFLOW;
	return -1;
      }
      strcpy(nodes[nodecount], node_list->name); 
      nodecount++;
    }

    node_list = node_list->next;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return nodecount;

}

int genders_getattr(genders_t handle, char *attrs[], char *vals[], int len, 
		    char *node) {

  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;
  int attrcount = 0;
  char *nodename;

  if (handle == NULL) {
    return -1;
  }

  if (attrs == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }
  
  if (node == NULL) {
    nodename = handle->nodename;
  }
  else {
    nodename = node;
  }

  /* find node */
  node_list = (struct node_listnode *)handle->nodes_head;
  while (node_list != NULL && strcmp(node_list->name,nodename) != 0) {
    node_list = node_list->next;
  }

  if (node_list == NULL) {
    handle->errnum = GENDERS_ERR_NOTFOUND;
    return -1;
  }

  attrval_list = (struct attrval_listnode *)node_list->attrvals_head;
  while (attrval_list != NULL) {
    if (attrcount >= len) {
      handle->errnum = GENDERS_ERR_OVERFLOW;
      return -1;
    }    

    strcpy(attrs[attrcount],attrval_list->name);
    if (vals != NULL && attrval_list->val != NULL) {
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

  if (handle == NULL) {
    return -1;
  }

  if (attrs == NULL || len <= 0) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }
  
  attr_list = (struct attr_listnode *)handle->attrs_head;
  while (attr_list != NULL) {
    strcpy(attrs[attrcount],attr_list->name);
    attrcount++;
    attr_list = attr_list->next;
  }
  
  if (attrcount != handle->numattrs) {
    handle->errnum = GENDERS_ERR_INTERNAL;
    return -1;
  }

  handle->errnum = GENDERS_ERR_SUCCESS;
  return attrcount;  
}

int genders_testattr(genders_t handle, char *node, char *attr, char *val) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;
  char *nodename;

  if (handle == NULL) {
    return -1;
  }

  if (attr == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }
  
  if (node == NULL) {
    nodename = handle->nodename;
  }
  else {
    nodename = node;
  }

  /* find node */
  node_list = (struct node_listnode *)handle->nodes_head;
  while (node_list != NULL && strcmp(node_list->name,nodename) != 0) {
    node_list = node_list->next;
  }

  if (node_list == NULL) {
    handle->errnum = GENDERS_ERR_NOTFOUND;
    return -1;
  }

  /* find attribute */
  attrval_list = (struct attrval_listnode *)node_list->attrvals_head;
  while (attrval_list != NULL) {
    
    if (strcmp(attrval_list->name,attr) == 0) {
      if (val != NULL && attrval_list->val != NULL) {
	strcpy(val,attrval_list->val);
      }
      handle->errnum = GENDERS_ERR_SUCCESS;
      return 1;  
    }
    attrval_list = attrval_list->next;
  }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_testattrval(genders_t handle, char *node, char *attr, char *val) {
  struct node_listnode *node_list;
  struct attrval_listnode *attrval_list;
  char *nodename;

  if (handle == NULL) {
    return -1;
  }

  if (attr == NULL || val == NULL) {
    handle->errnum = GENDERS_ERR_PARAMETERS;
    return -1;
  }
  
  if (node == NULL) {
    nodename = handle->nodename;
  }
  else {
    nodename = node;
  }

  /* find node */
  node_list = (struct node_listnode *)handle->nodes_head;
  while (node_list != NULL && strcmp(node_list->name,nodename) != 0) {
    node_list = node_list->next;
  }

  if (node_list == NULL) {
    handle->errnum = GENDERS_ERR_NOTFOUND;
    return -1;
  }

  /* find attribute & value */
  attrval_list = (struct attrval_listnode *)node_list->attrvals_head;
  while (attrval_list != NULL) {
    if ((strcmp(attrval_list->name,attr) == 0) &&
	(attrval_list->val != NULL) &&
	(strcmp(attrval_list->val,val) == 0)) {
      handle->errnum = GENDERS_ERR_SUCCESS;
      return 1;  
    }
    attrval_list = attrval_list->next;
  }
  
  handle->errnum = GENDERS_ERR_SUCCESS;
  return 0;
}

int genders_elapsedtime(genders_t handle) {

  time_t t;

  if (handle == NULL) {
    return -1;
  }
  else if (handle->time_genders_loaded == 0) {
    handle->errnum = GENDERS_ERR_NOTOPEN;
    return -1;
  }
  else {
    t = time(NULL);
    if (t == (time_t)-1) {
      handle->errnum = GENDERS_ERR_INTERNAL;
      return -1;
    }
    handle->errnum = GENDERS_ERR_SUCCESS;
    return (int)(t - handle->time_genders_loaded);
  }
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */