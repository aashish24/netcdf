/*********************************************************************
 *   Copyright 1993, UCAR/Unidata
 *   See netcdf/COPYRIGHT file for copying and redistribution conditions.
 *   $Header: /upc/share/CVS/netcdf-3/libncdap3/common34.c,v 1.29 2010/05/25 13:53:02 ed Exp $
 *********************************************************************/

#include "ncdap3.h"

#ifdef HAVE_GETRLIMIT
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include "dapdump.h"

extern CDFnode* v4node;

/* Define the set of protocols known to be constrainable */
static char* constrainableprotocols[] = {"http", "https",NULL};
static NCerror buildcdftree34r(NCDAPCOMMON*,OCobject,CDFnode*,CDFtree*,CDFnode**);
static void defdimensions(OCobject, CDFnode*, NCDAPCOMMON*, CDFtree*);
static NCerror  attachsubset34r(CDFnode*, CDFnode*);
static void free1cdfnode34(CDFnode* node);

/* Define Procedures that are common to both
   libncdap3 and libncdap4
*/

/* Ensure every node has an initial base name defined and fullname */
/* Exceptions: anonymous dimensions. */
static NCerror
fix1node34(NCDAPCOMMON* nccomm, CDFnode* node)
{
    if(node->nctype == NC_Dimension && node->ocname == NULL) return NC_NOERR;
    ASSERT((node->ocname != NULL));
    nullfree(node->ncbasename);
    node->ncbasename = cdflegalname3(node->ocname);
    if(node->ncbasename == NULL) return NC_ENOMEM;
    nullfree(node->ncfullname);
    node->ncfullname = makecdfpathstring3(node,nccomm->cdf.separator);
    if(node->ncfullname == NULL) return NC_ENOMEM;
    if(node->nctype == NC_Primitive)
        node->externaltype = nctypeconvert(nccomm,node->etype);
#ifdef IGNORE
    if(node->nctype == NC_Dimension)
        node->maxstringlength = nccomm->cdf.defaultstringlength;
#endif
    return NC_NOERR;
}

NCerror
fixnodes34(NCDAPCOMMON* nccomm, NClist* cdfnodes)
{
    int i;
    for(i=0;i<nclistlength(cdfnodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(cdfnodes,i);
	NCerror err = fix1node34(nccomm,node);
	if(err) return err;
    }
    return NC_NOERR;
}

#ifdef IGNORE
NCerror
computecdfinfo34(NCDAPCOMMON* nccomm, NClist* cdfnodes)
{
    int i;
    /* Ensure every node has an initial base name defined and fullname */
    /* Exceptions: anonymous dimensions. */
    for(i=0;i<nclistlength(cdfnodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(cdfnodes,i);
        if(node->nctype == NC_Dimension && node->name == NULL) continue;
	ASSERT((node->name != NULL));
        nullfree(node->ncbasename);
        node->ncbasename = cdflegalname3(node->name);
        nullfree(node->ncfullname);
	node->ncfullname = makecdfpathstring3(node,nccomm->cdf.separator);
if(node==v4node && node->ncfullname[0] != 'Q')dappanic("");
    }
    for(i=0;i<nclistlength(cdfnodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(cdfnodes,i);
        if(node->nctype == NC_Primitive)
            node->externaltype = nctypeconvert(node->etype);
    }
    for(i=0;i<nclistlength(cdfnodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(cdfnodes,i);
	if(node->nctype != NC_Dimension) continue;
	node->maxstringlength = nccomm->cdf.defaultstringlength;
    }
    return NC_NOERR;
}
#endif

NCerror
fixgrid34(NCDAPCOMMON* nccomm, CDFnode* grid)
{
    unsigned int i,glen;
    CDFnode* array;

    glen = nclistlength(grid->subnodes);
    array = (CDFnode*)nclistget(grid->subnodes,0);	        
    if(nccomm->controls.flags & (NCF_NC3)) {
        /* Rename grid Array: variable, but leave its oc base name alone */
        nullfree(array->ncbasename);
        array->ncbasename = nulldup(grid->ncbasename);
        if(!array->ncbasename) return NC_ENOMEM;
    }
    /* validate and modify the grid structure */
    if((glen-1) != nclistlength(array->array.dimset0)) goto invalid;
    for(i=1;i<glen;i++) {
	CDFnode* arraydim = (CDFnode*)nclistget(array->array.dimset0,i-1);
	CDFnode* map = (CDFnode*)nclistget(grid->subnodes,i);
	CDFnode* mapdim;
	/* map must have 1 dimension */
	if(nclistlength(map->array.dimset0) != 1) goto invalid;
	/* and the map name must match the ith array dimension */
	if(arraydim->ocname != NULL && map->ocname != NULL
	   && strcmp(arraydim->ocname,map->ocname) != 0)
	    goto invalid;
	/* and the map name must match its dim name (if any) */
	mapdim = (CDFnode*)nclistget(map->array.dimset0,0);
	if(mapdim->ocname != NULL && map->ocname != NULL
	   && strcmp(mapdim->ocname,map->ocname) != 0)
	    goto invalid;
	/* Add appropriate names for the anonymous dimensions */
	/* Do the map name first, so the array dim may inherit */
	if(mapdim->ocname == NULL) {
	    nullfree(mapdim->ncbasename);
	    mapdim->ocname = nulldup(map->ocname);
	    if(!mapdim->ocname) return NC_ENOMEM;
	    mapdim->ncbasename = cdflegalname3(mapdim->ocname);
	    if(!mapdim->ncbasename) return NC_ENOMEM;
	}
	if(arraydim->ocname == NULL) {
	    nullfree(arraydim->ncbasename);
	    arraydim->ocname = nulldup(map->ocname);
	    if(!arraydim->ocname) return NC_ENOMEM;
	    arraydim->ncbasename = cdflegalname3(arraydim->ocname);
	    if(!arraydim->ncbasename) return NC_ENOMEM;
	}
        if(FLAGSET(nccomm->controls,(NCF_NCDAP|NCF_NC3))) {
	    char tmp[3*NC_MAX_NAME];
            /* Add the grid name to the basename of the map */
	    snprintf(tmp,sizeof(tmp),"%s%s%s",map->container->ncbasename,
					  nccomm->cdf.separator,
					  map->ncbasename);
	    nullfree(map->ncbasename);
            map->ncbasename = nulldup(tmp);
	    if(!map->ncbasename) return NC_ENOMEM;
	}
    }
    return NC_NOERR;
invalid:
    return NC_EINVAL; /* mal-formed grid */
}


#ifdef IGNORE
static void
cloneseqdims(NCDAPCOMMON* nccomm, NClist* dimset, CDFnode* var)
{
    int i;
    for(i=0;i<nclistlength(dimset);i++) {
	CDFnode* dim = (CDFnode*)nclistget(dimset,i);
	if(DIMFLAG(dim,CDFDIMSEQ))
	    nclistset(dimset,i,(ncelem)clonedim(nccomm,dim,var));
    }
}
#endif


#ifdef IGNORE
/* Compute the dimsetall for the given node; do not assume
   parent container dimsetall is defined
 */
int
getcompletedimset3(CDFnode* var, NClist* dimset)
{
    int i,j;
    NClist* path = nclistnew();
    CDFnode* node;

    nclistclear(dimset);
    /* Get the inherited dimensions first*/
    collectnodepath3(var,path,WITHOUTDATASET);
    for(i=0;i<nclistlength(path)-1;i++) {
	node = (CDFnode*)nclistget(path,i);
	for(j=0;j<nclistlength(node->array.dimsetplus);jj++) {
	    CDFnode* dim = (CDFnode*)nclistget(node->array.dimset,j);
	    nclistpush(dimset,(ncelem)dim);
	}
    }
    inherited = nclistlength(dimset); /* mark the # of inherited dimensions */
    /* Now add the base dimensions */
    node = (CDFnode*)nclistpop(path);    
    for(j=0;j<nclistlength(node->array.dimsetplus);j++) {
	CDFnode* dim = (CDFnode*)nclistget(node->array.dimset0,j);
	nclistpush(dimset,(ncelem)dim);
    }
    nclistfree(path);
    return inherited;
}
#endif


/**
 *  Given an anonymous dimension, compute the
 *  effective 0-based index wrt to the specified var.
 *  The result should mimic the libnc-dap indices.
 */

static void
computedimindexanon3(CDFnode* dim, CDFnode* var)
{
    int i;
    NClist* dimset = var->array.dimsetall;
    for(i=0;i<nclistlength(dimset);i++) {
	CDFnode* candidate = (CDFnode*)nclistget(dimset,i);
        if(dim == candidate) {
	   dim->dim.index1=i+1;
	   return;
	}
    }
}

/* Replace dims in a list with their corresponding basedim */
static void
replacedims(NClist* dims)
{
    int i;
    for(i=0;i<nclistlength(dims);i++) {
        CDFnode* dim = (CDFnode*)nclistget(dims,i);
	CDFnode* basedim = dim->dim.basedim;
	if(basedim == NULL) continue;
	nclistset(dims,i,(ncelem)basedim);
    }
}

/**
 Two dimensions are equivalent if
 1. they have the same size
 2. neither are anonymous
 3. they ave the same names. 
 */
static int
equivalentdim(CDFnode* basedim, CDFnode* dupdim)
{
    if(dupdim->dim.declsize != basedim->dim.declsize) return 0;
    if(basedim->ocname == NULL && dupdim->ocname == NULL) return 0;
    if(basedim->ocname == NULL || dupdim->ocname == NULL) return 0;
    if(strcmp(dupdim->ocname,basedim->ocname) != 0) return 0;
    return 1;
}

/*
   Provide short and/or unified names for dimensions.
   This must mimic lib-ncdap, which is difficult.
*/
NCerror
computecdfdimnames34(NCDAPCOMMON* nccomm)
{
    int i,j;
    char tmp[NC_MAX_NAME*2];
    NClist* conflicts = nclistnew();
    NClist* varnodes = nccomm->cdf.varnodes;
    NClist* alldims;
    NClist* basedims;
    
    /* Collect all dimension nodes from dimsetall lists */

    alldims = getalldims34(nccomm,0);    

    /* Assign an index to all anonymous dimensions
       vis-a-vis its containing variable
    */
    for(i=0;i<nclistlength(varnodes);i++) {
	CDFnode* var = (CDFnode*)nclistget(varnodes,i);
        for(j=0;j<nclistlength(var->array.dimsetall);j++) {
	    CDFnode* dim = (CDFnode*)nclistget(var->array.dimsetall,j);
	    if(dim->ocname != NULL) continue; /* not anonymous */
 	    computedimindexanon3(dim,var);
	}
    }

    /* Unify dimensions by defining one dimension as the "base"
       dimension, and make all "equivalent" dimensions point to the
       base dimension.
	1. Equivalent means: same size and both have identical non-null names.
	2. Dims with same name but different sizes will be handled separately
    */
    for(i=0;i<nclistlength(alldims);i++) {
	CDFnode* dupdim = NULL;
	CDFnode* basedim = (CDFnode*)nclistget(alldims,i);
	if(basedim == NULL) continue;
	if(basedim->dim.basedim != NULL) continue; /* already processed*/
	for(j=i+1;j<nclistlength(alldims);j++) { /* Sigh, n**2 */
	    dupdim = (CDFnode*)nclistget(alldims,j);
	    if(basedim == dupdim) continue;
	    if(dupdim == NULL) continue;
	    if(dupdim->dim.basedim != NULL) continue; /* already processed */
	    if(!equivalentdim(basedim,dupdim))
		continue;
            dupdim->dim.basedim = basedim; /* equate */
#ifdef DEBUG1
fprintf(stderr,"assign: %s/%s -> %s/%s\n",
basedim->dim.array->ocname,basedim->ocname,
dupdim->dim.array->ocname,dupdim->ocname
);
#endif
	}
    }

    /* Next case: same name and different sizes*/
    /* => rename second dim by appending a counter */

    for(i=0;i<nclistlength(alldims);i++) {
	CDFnode* basedim = (CDFnode*)nclistget(alldims,i);
	if(basedim->dim.basedim != NULL) continue; /* ignore*/
	/* Collect all conflicting dimensions */
	nclistclear(conflicts);
        for(j=i+1;j<nclistlength(alldims);j++) {
	    CDFnode* dim = (CDFnode*)nclistget(alldims,j);
	    if(dim->dim.basedim != NULL) continue; /* ignore*/	    
	    if(dim->ocname == NULL && basedim->ocname == NULL) continue;
	    if(dim->ocname == NULL || basedim->ocname == NULL) continue;
	    if(strcmp(dim->ocname,basedim->ocname)!=0) continue;
	    if(dim->dim.declsize == basedim->dim.declsize) continue;
	    nclistpush(conflicts,(ncelem)dim);
	}
	/* Give  all the conflicting dimensions an index */
	for(j=0;j<nclistlength(conflicts);j++) {
	    CDFnode* dim = (CDFnode*)nclistget(conflicts,j);
	    dim->dim.index1 = j+1;
	}
    }
    nclistfree(conflicts);

    /* Replace all non-base dimensions with their base dimension */
    for(i=0;i<nclistlength(varnodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(varnodes,i);
	replacedims(node->array.dimsetall);
	replacedims(node->array.dimsetplus);
	replacedims(node->array.dimset0);
    }

    /* Collect list of all basedims */
    basedims = nclistnew();
    for(i=0;i<nclistlength(alldims);i++) {
	CDFnode* dim = (CDFnode*)nclistget(alldims,i);
	if(dim->dim.basedim == NULL) {
	    if(!nclistcontains(basedims,(ncelem)dim)) {
		nclistpush(basedims,(ncelem)dim);
	    }
	}
    }

    nccomm->cdf.dimnodes = basedims;

    /* cleanup */
    nclistfree(alldims);

#ifdef IGNORE
    /* Process record dim */
    if(nccomm->cdf.unlimited != NULL && DIMFLAG(nccomm->cdf.unlimited,CDFDIMRECORD)) {
	CDFnode* recdim = nccomm->cdf.unlimited;
	for(i=0;i<nclistlength(alldims);i++) {
	    int match;
	    CDFnode* dupdim = (CDFnode*)nclistget(alldims,i);
	    if(dupdim->dim.basedim != NULL) continue; /* already processed */
	    match = (strcmp(dupdim->ncfullname,recdim->ncfullname) == 0
	             && dupdim->dim.declsize == recdim->dim.declsize);
            if(match) {
	        dupdim->dim.basedim = recdim;
	    }
	}
    }
#endif

    /* Assign ncbasenames and ncfullnames to base dimensions */
    for(i=0;i<nclistlength(basedims);i++) {
	CDFnode* dim = (CDFnode*)nclistget(basedims,i);
	CDFnode* var = dim->dim.array;
	if(dim->dim.basedim != NULL) PANIC1("nonbase basedim: %s\n",dim->ocname);
	/* stringdim names are already assigned */
	if(dim->ocname == NULL) { /* anonymous: use the index to compute the name */
            snprintf(tmp,sizeof(tmp),"%s_%d",
                            var->ncfullname,dim->dim.index1-1);
            nullfree(dim->ncbasename);
            dim->ncbasename = cdflegalname3(tmp);
            nullfree(dim->ncfullname);
            dim->ncfullname = nulldup(dim->ncbasename);
    	} else { /* !anonymous */
	    nullfree(dim->ncbasename);
	    dim->ncbasename = cdflegalname3(dim->ocname);
    	    nullfree(dim->ncfullname);
	    dim->ncfullname = nulldup(dim->ncbasename);
	}
     }

    /* Verify unique and defined names for dimensions*/
    for(i=0;i<nclistlength(basedims);i++) {
	CDFnode* dim1 = (CDFnode*)nclistget(basedims,i);
	if(dim1->dim.basedim != NULL) continue;
	if(dim1->ncbasename == NULL || dim1->ncfullname == NULL)
	    PANIC1("missing dim names: %s",dim1->ocname);
	for(j=0;j<i;j++) {
	    CDFnode* dim2 = (CDFnode*)nclistget(basedims,j);
	    if(dim2->dim.basedim != NULL) continue;
	    if(strcmp(dim1->ncfullname,dim2->ncfullname)==0) {
		PANIC1("duplicate dim names: %s",dim1->ncfullname);
	    }
	}
    }

#ifdef DEBUG
for(i=0;i<nclistlength(basedims);i++) {
CDFnode* dim = (CDFnode*)nclistget(basedims,i);
fprintf(stderr,"basedim: %s=%ld\n",dim->ncfullname,(long)dim->dim.declsize);
 }
#endif

    return NC_NOERR;
}

NCerror
makegetvar34(NCDAPCOMMON* nccomm, CDFnode* var, void* data, nc_type dsttype, Getvara** getvarp)
{
    Getvara* getvar;
    NCerror ncstat = NC_NOERR;

    getvar = (Getvara*)calloc(1,sizeof(Getvara));
    MEMCHECK(getvar,NC_ENOMEM);
    if(getvarp) *getvarp = getvar;

    getvar->target = var;
    getvar->memory = data;
    getvar->dsttype = dsttype;
    getvar->target = var;
    if(ncstat) nullfree(getvar);
    return ncstat;
}

int
constrainable34(NC_URI* durl)
{
   char** protocol = constrainableprotocols;
   for(;*protocol;protocol++) {
	if(strcmp(durl->protocol,*protocol)==0)
	    return 1;
   }
   return 0;
}

CDFnode*
makecdfnode34(NCDAPCOMMON* nccomm, char* name, OCtype octype,
             /*optional*/ OCobject ocnode, CDFnode* container)
{
    CDFnode* node;
    assert(nccomm != NULL);
    node = (CDFnode*)calloc(1,sizeof(CDFnode));
    if(node == NULL) return (CDFnode*)NULL;

    node->ocname = NULL;
    if(name) {
        size_t len = strlen(name);
        if(len >= NC_MAX_NAME) len = NC_MAX_NAME-1;
        node->ocname = (char*)malloc(len+1);
	if(node->ocname == NULL) return NULL;
	memcpy(node->ocname,name,len);
	node->ocname[len] = '\0';
    }
    node->nctype = octypetonc(octype);
    node->dds = ocnode;
    node->subnodes = nclistnew();
    node->container = container;
    if(ocnode != OCNULL) {
	oc_inq_primtype(nccomm->oc.conn,ocnode,&octype);
        node->etype = octypetonc(octype);
    }
    return node;
}

/* Given an OCnode tree, mimic it as a CDFnode tree;
   Add DAS attributes if DAS is available. Accumulate set
   of all nodes in preorder.
*/
NCerror
buildcdftree34(NCDAPCOMMON* nccomm, OCobject ocroot, OCdxd occlass, CDFnode** cdfrootp)
{
    CDFnode* root = NULL;
    CDFtree* tree = (CDFtree*)calloc(1,sizeof(CDFtree));
    NCerror err = NC_NOERR;
    tree->ocroot = ocroot;
    tree->nodes = nclistnew();
    tree->occlass = occlass;
    tree->owner = nccomm;

    err = buildcdftree34r(nccomm,ocroot,NULL,tree,&root);
    if(!err) {
	if(occlass != OCDAS)
	    fixnodes34(nccomm,tree->nodes);
	if(cdfrootp) *cdfrootp = root;
    }
    return err;
}        

static NCerror
buildcdftree34r(NCDAPCOMMON* nccomm, OCobject ocnode, CDFnode* container,
                CDFtree* tree, CDFnode** cdfnodep)
{
    unsigned int i,ocrank,ocnsubnodes;
    OCtype octype;
    char* ocname = NULL;
    NCerror ncerr = NC_NOERR;
    CDFnode* cdfnode;

    oc_inq_class(nccomm->oc.conn,ocnode,&octype);
    oc_inq_name(nccomm->oc.conn,ocnode,&ocname);
    oc_inq_rank(nccomm->oc.conn,ocnode,&ocrank);
    oc_inq_nsubnodes(nccomm->oc.conn,ocnode,&ocnsubnodes);

    switch (octype) {
    case OC_Dataset:
    case OC_Grid:
    case OC_Structure:
    case OC_Sequence:
    case OC_Primitive:
	cdfnode = makecdfnode34(nccomm,ocname,octype,ocnode,container);
	nclistpush(tree->nodes,(ncelem)cdfnode);
	if(tree->root == NULL) {
	    tree->root = cdfnode;
	    cdfnode->tree = tree;
	}		
	break;

    case OC_Dimension:
    default: PANIC1("buildcdftree: unexpect OC node type: %d",(int)octype);

    }    
    /* cross link */
    cdfnode->root = tree->root;

    if(ocrank > 0) defdimensions(ocnode,cdfnode,nccomm,tree);
    for(i=0;i<ocnsubnodes;i++) {
	OCobject ocsubnode;
	CDFnode* subnode;
	oc_inq_ith(nccomm->oc.conn,ocnode,i,&ocsubnode);
	ncerr = buildcdftree34r(nccomm,ocsubnode,cdfnode,tree,&subnode);
	if(ncerr) return ncerr;
	nclistpush(cdfnode->subnodes,(ncelem)subnode);
    }
    nullfree(ocname);
    if(cdfnodep) *cdfnodep = cdfnode;
    return ncerr;
}

static void
defdimensions(OCobject ocnode, CDFnode* cdfnode, NCDAPCOMMON* nccomm, CDFtree* tree)
{
    unsigned int i,ocrank;
 
    oc_inq_rank(nccomm->oc.conn,ocnode,&ocrank);
    assert(ocrank > 0);
    for(i=0;i<ocrank;i++) {
	CDFnode* cdfdim;
	OCobject ocdim;
	char* ocname;
	size_t declsize;

	oc_inq_ithdim(nccomm->oc.conn,ocnode,i,&ocdim);
	oc_inq_dim(nccomm->oc.conn,ocdim,&declsize,&ocname);

	cdfdim = makecdfnode34(nccomm,ocname,OC_Dimension,
                              ocdim,cdfnode->container);
	nullfree(ocname);
	nclistpush(tree->nodes,(ncelem)cdfdim);
	/* Initially, constrained and unconstrained are same */
	cdfdim->dim.declsize = declsize;
#ifdef IGNORE
	cdfdim->dim.declsize0 = declsize;
#endif
	cdfdim->dim.array = cdfnode;
	if(cdfnode->array.dimset0 == NULL) 
	    cdfnode->array.dimset0 = nclistnew();
	nclistpush(cdfnode->array.dimset0,(ncelem)cdfdim);
    }    
}

/* Note: this routine only applies some common
   client parameters, other routines may apply
   specific ones.
*/

NCerror
applyclientparams34(NCDAPCOMMON* nccomm)
{
    int i,len;
    int dfaltstrlen = DEFAULTSTRINGLENGTH;
    int dfaltseqlim = DEFAULTSEQLIMIT;
    const char* value;
    char tmpname[NC_MAX_NAME+32];
    char* pathstr;
    OCconnection conn = nccomm->oc.conn;
    unsigned long limit;

    nccomm->cdf.cache->cachelimit = DFALTCACHELIMIT;
    value = oc_clientparam_get(conn,"cachelimit");
    limit = getlimitnumber(value);
    if(limit > 0) nccomm->cdf.cache->cachelimit = limit;

    nccomm->cdf.fetchlimit = DFALTFETCHLIMIT;
    value = oc_clientparam_get(conn,"fetchlimit");
    limit = getlimitnumber(value);
    if(limit > 0) nccomm->cdf.fetchlimit = limit;

    nccomm->cdf.smallsizelimit = DFALTSMALLLIMIT;
    value = oc_clientparam_get(conn,"smallsizelimit");
    limit = getlimitnumber(value);
    if(limit > 0) nccomm->cdf.smallsizelimit = limit;

    nccomm->cdf.cache->cachecount = DFALTCACHECOUNT;
#ifdef HAVE_GETRLIMIT
    { struct rlimit rl;
      if(getrlimit(RLIMIT_NOFILE, &rl) >= 0) {
	nccomm->cdf.cache->cachecount = (size_t)(rl.rlim_cur / 2);
      }
    }
#endif
    value = oc_clientparam_get(conn,"cachecount");
    limit = getlimitnumber(value);
    if(limit > 0) nccomm->cdf.cache->cachecount = limit;
    /* Ignore limit if not caching */
    if(!FLAGSET(nccomm->controls,NCF_CACHE))
        nccomm->cdf.cache->cachecount = 0;

    if(oc_clientparam_get(conn,"nolimit") != NULL)
	dfaltseqlim = 0;
    value = oc_clientparam_get(conn,"limit");
    if(value != NULL && strlen(value) != 0) {
        if(sscanf(value,"%d",&len) && len > 0) dfaltseqlim = len;
    }
    nccomm->cdf.defaultsequencelimit = dfaltseqlim;

    /* allow embedded _ */
    value = oc_clientparam_get(conn,"stringlength");
    if(value != NULL && strlen(value) != 0) {
        if(sscanf(value,"%d",&len) && len > 0) dfaltstrlen = len;
    }
    nccomm->cdf.defaultstringlength = dfaltstrlen;

    /* String dimension limits apply to variables */
    for(i=0;i<nclistlength(nccomm->cdf.varnodes);i++) {
	CDFnode* var = (CDFnode*)nclistget(nccomm->cdf.varnodes,i);
	/* Define the client param stringlength for this variable*/
	var->maxstringlength = 0; /* => use global dfalt */
	strcpy(tmpname,"stringlength_");
	pathstr = makeocpathstring3(conn,var->dds,".");
	strcat(tmpname,pathstr);
	nullfree(pathstr);
	value = oc_clientparam_get(conn,tmpname);	
        if(value != NULL && strlen(value) != 0) {
            if(sscanf(value,"%d",&len) && len > 0) var->maxstringlength = len;
	}
    }
    /* Sequence limits apply to sequences */
    for(i=0;i<nclistlength(nccomm->cdf.ddsroot->tree->nodes);i++) {
	CDFnode* var = (CDFnode*)nclistget(nccomm->cdf.ddsroot->tree->nodes,i);
	if(var->nctype != NC_Sequence) continue;
	var->sequencelimit = dfaltseqlim;
	strcpy(tmpname,"nolimit_");
	pathstr = makeocpathstring3(conn,var->dds,".");
	strcat(tmpname,pathstr);
	if(oc_clientparam_get(conn,tmpname) != NULL)
	    var->sequencelimit = 0;
	strcpy(tmpname,"limit_");
	strcat(tmpname,pathstr);
	value = oc_clientparam_get(conn,tmpname);
        if(value != NULL && strlen(value) != 0) {
            if(sscanf(value,"%d",&len) && len > 0)
		var->sequencelimit = len;
	}
	nullfree(pathstr);
    }
    return NC_NOERR;
}

void
freecdfroot34(CDFnode* root)
{
    int i;
    CDFtree* tree;
    NCDAPCOMMON* nccomm;
    if(root == NULL) return;
    tree = root->tree;
    ASSERT((tree != NULL));
    /* Explicitly FREE the ocroot */
    nccomm = tree->owner;
    oc_root_free(nccomm->oc.conn,tree->ocroot);
    tree->ocroot = OCNULL;
    for(i=0;i<nclistlength(tree->nodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(tree->nodes,i);
	free1cdfnode34(node);
    }
    nclistfree(tree->nodes);
    nullfree(tree);
}

/* Free up a single node, but not any
   nodes it points to.
*/  
static void
free1cdfnode34(CDFnode* node)
{
    unsigned int j,k;
    if(node == NULL) return;
    nullfree(node->ocname);
    nullfree(node->ncbasename);
    nullfree(node->ncfullname);
    if(node->attributes != NULL) {
	for(j=0;j<nclistlength(node->attributes);j++) {
	    NCattribute* att = (NCattribute*)nclistget(node->attributes,j);
	    nullfree(att->name);
	    for(k=0;k<nclistlength(att->values);k++)
		nullfree((char*)nclistget(att->values,k));
	    nclistfree(att->values);
	    nullfree(att);
	}
    }
    nullfree(node->dodsspecial.dimname);
    nclistfree(node->subnodes);
    nclistfree(node->attributes);
    nclistfree(node->array.dimsetplus);
    nclistfree(node->array.dimsetall);
    nclistfree(node->array.dimset0);

    /* Clean up the ncdap4 fields also */
    nullfree(node->typename);
    nullfree(node->vlenname);
    nullfree(node);
}

/* Return true if node and node1 appear to refer to the same thing;
   takes grid->structure changes into account.
*/
int
nodematch34(CDFnode* node1, CDFnode* node2)
{
    return simplenodematch34(node1,node2);
}

int
simplenodematch34(CDFnode* node1, CDFnode* node2)
{
    if(node1 == NULL) return (node2==NULL);
    if(node2 == NULL) return 0;
    if(node1->nctype != node2->nctype) {
	/* Check for Grid->Structure match */
	if((node1->nctype == NC_Structure && node2->nctype == NC_Grid)
	   || (node2->nctype == NC_Structure && node1->nctype == NC_Grid)){
	   if(node1->ocname == NULL || node2->ocname == NULL
	      || strcmp(node1->ocname,node2->ocname) !=0) return 0;	    	
	} else return 0;
    }
    /* Add hack to address the screwed up Columbia server */
    if(node1->nctype == NC_Dataset) return 1;
    if(node1->nctype == NC_Primitive
       && node1->etype != node2->etype) return 0;
    if(node1->ocname != NULL && node2->ocname != NULL
       && strcmp(node1->ocname,node2->ocname)!=0) return 0;
    if(nclistlength(node1->array.dimset0)
       != nclistlength(node2->array.dimset0)) return 0;
    return 1;
}

/*
Given DDS node, locate the node
in a DATADDS that matches the DDS node.
Return NULL if no node found
*/

#ifdef IGNORE
static CDFnode*
findxnode34r(NClist* path, int depth, CDFnode* xnode)
{
    unsigned int i;
    CDFnode* pathnode;
    unsigned int len = nclistlength(path);
    int lastnode = (depth == (len - 1));

    if(depth >= len) return NULL;

    pathnode = (CDFnode*)nclistget(path,depth);

    /* If this path element matches the current xnode, then recurse */
    if(nodematch34(pathnode,xnode)) {
        if(lastnode) return xnode;
        for(i=0;i<nclistlength(xnode->subnodes);i++) {
	    CDFnode* xsubnode = (CDFnode*)nclistget(xnode->subnodes,i);
	    CDFnode* matchnode;
	    matchnode = findxnode34r(path,depth+1,xsubnode);	    
	    if(matchnode != NULL) return matchnode;
	}
    } else
    /* Ok, we have a node mismatch; normally return NULL,
       but must handle the special case of an elided Grid.
    */
    if(pathnode->nctype == NC_Grid && xnode->nctype == NC_Primitive) {
	/* Try to match the xnode to one of the subparts of the grid */
	CDFnode* matchnode;
	matchnode = findxnode34r(path,depth+1,xnode);	    
	if(matchnode != NULL) return matchnode;
    }
    /* Could not find node, return NULL */
    return NULL;
}


CDFnode*
findxnode34(CDFnode* target, CDFnode* xroot)
{
    CDFnode* xtarget = NULL;
    NClist* path = nclistnew();
    collectnodepath3(target,path,WITHDATASET);
    xtarget = findxnode34r(path,0,xroot);
    nclistfree(path);
    return xtarget;
}
#endif

void
unattach34(CDFnode* root)
{
    unsigned int i;
    CDFtree* xtree = root->tree;
    for(i=0;i<nclistlength(xtree->nodes);i++) {
	CDFnode* xnode = (CDFnode*)nclistget(xtree->nodes,i);
	/* break bi-directional link */
        xnode->attachment = NULL;
    }
}

static void
setattach(CDFnode* target, CDFnode* template)
{
    target->attachment = template;
    template->attachment = target;
    /* Transfer important information */
    target->externaltype = template->externaltype;
    target->maxstringlength = template->maxstringlength;
    target->sequencelimit = template->sequencelimit;
    target->ncid = template->ncid;
    /* also transfer libncdap4 info */
    target->typeid = template->typeid;
    target->typesize = template->typesize;
}

static NCerror
attachdims34(CDFnode* xnode, CDFnode* ddsnode)
{
    unsigned int i;
    for(i=0;i<nclistlength(xnode->array.dimsetall);i++) {
	CDFnode* xdim = (CDFnode*)nclistget(xnode->array.dimsetall,i);
	CDFnode* ddim = (CDFnode*)nclistget(ddsnode->array.dimsetall,i);
	setattach(xdim,ddim);
#ifdef DEBUG2
fprintf(stderr,"attachdim: %s->%s\n",xdim->ocname,ddim->ocname);
#endif
    }
    return NC_NOERR;
}

/* 
Match a DATADDS node to a DDS node.
It is assumed that both trees have been regridded if necessary.
*/

static NCerror
attach34r(CDFnode* xnode, NClist* path, int depth)
{
    unsigned int i,plen,lastnode,gridable;
    NCerror ncstat = NC_NOERR;
    CDFnode* pathnode;
    CDFnode* pathnext;

    plen = nclistlength(path);
    if(depth >= plen) {THROWCHK(ncstat=NC_EINVAL); goto done;}

    lastnode = (depth == (plen-1));
    pathnode = (CDFnode*)nclistget(path,depth);
    ASSERT((simplenodematch34(xnode,pathnode)));
    setattach(xnode,pathnode);    
#ifdef DEBUG2
fprintf(stderr,"attachnode: %s->%s\n",xnode->ocname,pathnode->ocname);
#endif

    if(lastnode) goto done; /* We have the match and are done */

    if(nclistlength(xnode->array.dimsetall) > 0) {
	attachdims34(xnode,pathnode);
    }

    ASSERT((!lastnode));
    pathnext = (CDFnode*)nclistget(path,depth+1);

    gridable = (pathnext->nctype == NC_Grid && depth+2 < plen);

    /* Try to find an xnode subnode that matches pathnext */
    for(i=0;i<nclistlength(xnode->subnodes);i++) {
        CDFnode* xsubnode = (CDFnode*)nclistget(xnode->subnodes,i);
        if(simplenodematch34(xsubnode,pathnext)) {
	    ncstat = attach34r(xsubnode,path,depth+1);
	    if(ncstat) goto done;
        } else if(gridable && xsubnode->nctype == NC_Primitive) {
            /* grids may or may not appear in the datadds;
	       try to match the xnode subnodes against the parts of the grid
	    */
   	    CDFnode* pathnext2 = (CDFnode*)nclistget(path,depth+2);
	    if(simplenodematch34(xsubnode,pathnext2)) {
	        ncstat = attach34r(xsubnode,path,depth+2);
                if(ncstat) goto done;
	    }
	}
    }
done:
    return THROW(ncstat);
}

NCerror
attach34(CDFnode* xroot, CDFnode* ddstarget)
{
    NCerror ncstat = NC_NOERR;
    NClist* path = nclistnew();
    CDFnode* ddsroot = ddstarget->root;

    if(xroot->attachment) unattach34(xroot);
    if(ddsroot != NULL && ddsroot->attachment) unattach34(ddsroot);
    if(!simplenodematch34(xroot,ddsroot))
	{THROWCHK(ncstat=NC_EINVAL); goto done;}
    collectnodepath3(ddstarget,path,WITHDATASET);
    ncstat = attach34r(xroot,path,0);
done:
    nclistfree(path);
    return ncstat;
}

/* 
Match nodes in template tree to nodes in target tree;
template tree is typically a structural superset of target tree.
WARNING: Dimensions are not attached 
*/

NCerror
attachsubset34(CDFnode* target, CDFnode* template)
{
    NCerror ncstat = NC_NOERR;

    if(template == NULL) {THROWCHK(ncstat=NC_NOERR); goto done;}
    if(!nodematch34(target,template)) {THROWCHK(ncstat=NC_EINVAL); goto done;}
#ifdef DEBUG2
fprintf(stderr,"attachsubset: target=%s\n",dumptree(target));
fprintf(stderr,"attachsubset: template=%s\n",dumptree(template));
#endif
    ncstat = attachsubset34r(target,template);
done:
    return ncstat;
}

static NCerror
attachsubset34r(CDFnode* target, CDFnode* template)
{
    unsigned int i;
    NCerror ncstat = NC_NOERR;
    int fieldindex;

#ifdef DEBUG2
fprintf(stderr,"attachsubsetr: attach: target=%s template=%s\n",
	target->ocname,template->ocname);
#endif

    ASSERT((nodematch34(target,template)));
    setattach(target,template);

    /* Try to match target subnodes against template subnodes */

    fieldindex = 0;
    for(fieldindex=0,i=0;i<nclistlength(template->subnodes) && fieldindex<nclistlength(target->subnodes);i++) {
        CDFnode* templatesubnode = (CDFnode*)nclistget(template->subnodes,i);
        CDFnode* targetsubnode = (CDFnode*)nclistget(target->subnodes,fieldindex);
        if(nodematch34(targetsubnode,templatesubnode)) {
#ifdef DEBUG2
fprintf(stderr,"attachsubsetr: match: %s :: %s\n",targetsubnode->ocname,templatesubnode->ocname);
#endif
            ncstat = attachsubset34r(targetsubnode,templatesubnode);
   	    if(ncstat) goto done;
	    fieldindex++;
	}
    }
done:
    return THROW(ncstat);
}

#ifdef IGNORE
/* Attach all dstnodes to all templates; all dstnodes must match */
static NCerror
attachall34r(CDFnode* dstnode, CDFnode* srcnode)
{
    unsigned int i;
    NCerror ncstat = NC_NOERR;

    ASSERT((nodematch34(dstnode,srcnode)));
    setattach(dstnode,srcnode);    

    if(dstnode->array.rank > 0) {
	attachdims34(dstnode,srcnode);
    }

    /* Try to match dstnode subnodes against srcnode subnodes */
    if(nclistlength(dstnode->subnodes) != nclistlength(srcnode->subnodes))
	{THROWCHK(ncstat=NC_EINVAL); goto done;}

    for(i=0;i<nclistlength(dstnode->subnodes);i++) {
        CDFnode* dstsubnode = (CDFnode*)nclistget(dstnode->subnodes,i);
        CDFnode* srcsubnode = (CDFnode*)nclistget(srcnode->subnodes,i);
        if(!nodematch34(dstsubnode,srcsubnode))
	    {THROWCHK(ncstat=NC_EINVAL); goto done;}
        ncstat = attachall34r(dstsubnode,srcsubnode);
	if(ncstat) goto done;
    }
done:
    return THROW(ncstat);
}

/* 
Match nodes in one tree to nodes in another.
Usually used to attach the DATADDS to the DDS,
but not always.
*/
NCerror
attachall34(CDFnode* dstroot, CDFnode* srcroot)
{
    NCerror ncstat = NC_NOERR;

    if(dstroot->attachment) unattach34(dstroot);
    if(srcroot != NULL && srcroot->attachment) unattach34(srcroot);
    if(!nodematch34(dstroot,srcroot)) {THROWCHK(ncstat=NC_EINVAL); goto done;}
    ncstat = attachall34r(dstroot,srcroot);
done:
    return ncstat;
}
#endif



static void
getalldims34a(NClist* dimset, NClist* alldims)
{
    int i;
    for(i=0;i<nclistlength(dimset);i++) {
	CDFnode* dim = (CDFnode*)nclistget(dimset,i);
	if(!nclistcontains(alldims,(ncelem)dim))
	    nclistpush(alldims,(ncelem)dim);
    }
}

/* Accumulate a set of all the known dimensions
   vis-a-vis defined variables
*/
NClist*
getalldims34(NCDAPCOMMON* nccomm, int visibleonly)
{
    int i;
    NClist* alldims = nclistnew();
    NClist* varnodes = nccomm->cdf.varnodes;

    /* get bag of all dimensions */
    for(i=0;i<nclistlength(varnodes);i++) {
	CDFnode* node = (CDFnode*)nclistget(varnodes,i);
	if(!visibleonly || node->visible) {
	    getalldims34a(node->array.dimsetall,alldims);
	}
    }
    return alldims;
}
