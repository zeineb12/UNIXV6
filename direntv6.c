/**
 * @file direntv6.c
 * @brief accessing the UNIX v6 filesystem -- directory layer
 */
#include <string.h>
#include <stdlib.h>
#include "direntv6.h"
#include "unixv6fs.h"
#include "error.h"
#include "filev6.h"
#include "inode.h"

/**
 * @brief opens a directory reader for the specified inode 'inr'
 * @param u the mounted filesystem
 * @param inr the inode -- which must point to an allocated directory
 * @param d the directory reader (OUT)
 * @return 0 on success; <0 on errror
 */
int direntv6_opendir(const struct unix_filesystem *u, uint16_t inr, struct directory_reader *d)
{
    int err=-1;
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(d);

    memset(d,0,sizeof(struct directory_reader));
    if((err=filev6_open(u,inr,&d->fv6))<0) return err;
    if(!(d->fv6.i_node.i_mode & IALLOC)&&(((d->fv6.i_node.i_mode)&IFMT)!=IFDIR))return ERR_UNALLOCATED_INODE;
    if(((d->fv6.i_node.i_mode)&IFMT)!=IFDIR)return ERR_INVALID_DIRECTORY_INODE;
    d->cur=0;
    d->last=0;
    return 0;
}

/**
 * @brief return the next directory entry.
 * @param d the dierctory reader
 * @param name pointer to at least DIRENTMAX_LEN+1 bytes.  Filled in with the NULL-terminated string of the entry (OUT)
 * @param child_inr pointer to the inode number in the entry (OUT)
 * @return 1 on success;  0 if there are no more entries to read; <0 on error
 */
int direntv6_readdir(struct directory_reader *d, char *name, uint16_t *child_inr)
{
    M_REQUIRE_NON_NULL(d);
    M_REQUIRE_NON_NULL(name);
    M_REQUIRE_NON_NULL(child_inr);


    if(d->cur==d->last) {
        char buf[SECTOR_SIZE];
        memset(buf,0,SECTOR_SIZE);
        int r=filev6_readblock(&(d->fv6),buf);
        if(r<=0) return r;
        else {
            memcpy(d->dirs,buf, DIRENTRIES_PER_SECTOR * sizeof(struct direntv6));
            d->last+=(uint32_t)(r/sizeof(struct direntv6));
        }
    }
    memset(name,0,DIRENT_MAXLEN+1);
    strncpy(name,d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_name,DIRENT_MAXLEN);
    name[DIRENT_MAXLEN] = '\0';
    memset(child_inr,0,sizeof(uint16_t));
    *child_inr=d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_inumber;

    ++d->cur;
    return 1;
}

/**
 * @brief debugging routine; print the subtree (note: recursive)
 * @param u a mounted filesystem
 * @param inr the root of the subtree
 * @param prefix the prefix to the subtree
 * @return 0 on success; <0 on error
 */
int direntv6_print_tree(const struct unix_filesystem *u, uint16_t inr, const char *prefix)
{
    M_REQUIRE_NON_NULL(u);
    M_REQUIRE_NON_NULL(prefix);


    struct directory_reader d;
    memset(&d, 0, sizeof(struct directory_reader));
    int err=direntv6_opendir(u,inr,&d);
    //Si opendir retourne un entier négatif
    if(err<0) {
        //il s'agit d'une erreur à propager
        if(err!=ERR_INVALID_DIRECTORY_INODE) return err;
        //ils'agit d'un fichier plutot qu'un dossier
        else printf("%s %s\n", SHORT_FIL_NAME,prefix);
    }


    //il s'agit d'un dossier
    else {
        /* on imprime le prefix du dossier courant */
        printf("%s %s%c\n", SHORT_DIR_NAME,prefix,PATH_TOKEN);
        /* chaine de charactères qui va contenir le nom du direntv6 fils terminé par '\0' */
        char child_name[DIRENT_MAXLEN+1];
        memset(child_name,0,DIRENT_MAXLEN+1);
        child_name[0]='\0';
        /* entier qui va contenir le numéro d'inode du direntv6 fils */
        uint16_t child_inumber=0;


        int child_index=0;
        while((child_index=direntv6_readdir(&d,child_name,&child_inumber))>0) {
            /* chaine de charactères contenant le prefix du prochain fils */
            char next_prefix[MAXPATHLEN_UV6+1];
            memset(next_prefix,0,MAXPATHLEN_UV6+1);
            strcpy(next_prefix, prefix);
            next_prefix[strlen(prefix)] = '\0';
            /* on rajoute le char '/' à la fin */
            if (strlen(next_prefix) < MAXPATHLEN_UV6) next_prefix[strlen(next_prefix)] = PATH_TOKEN;
            /* on rajoute le nom du child si la taille maximale peut le contenir sinon on le concatène */
            if (MAXPATHLEN_UV6 - strlen(next_prefix) > strlen(child_name)) {
                strncat(next_prefix,child_name,strlen(child_name));
                next_prefix[strlen(next_prefix)] = '\0';
            } else {
                int max_chars_to_write=MAXPATHLEN_UV6-strlen(next_prefix);
                strncat(next_prefix,child_name,max_chars_to_write);
                next_prefix[strlen(next_prefix)] = '\0';
            }
            int error=direntv6_print_tree(u,child_inumber,next_prefix);
            if (error<0) return error;
        }
        if(child_index<0)return child_index;
    }
    return 0;
}

/**
 * @brief get the inode number for the given path
 * @param u a mounted filesystem
 * @param inr the current of the subtree
 * @param entry the prefix to the subtree
 * @param length legth of the entry
 * @return inr on success; <0 on error
 */

int direntv6_dirlookup_core(const struct unix_filesystem *u, uint16_t inr, const char *entry, size_t length)
{
	/* On retire tous les / initiaux */
	while(entry[0]=='/') entry++;
	/* Si on est arrivé au dossier ou fichier voulu et donc on retourne le numéro d'inode */
	if(strlen(entry)==0) return inr;
	struct directory_reader d;
	int err=0;
	if ((err=direntv6_opendir(u,inr,&d))<0) return err;
	/*Nom du prochain dossier ou fichier à traiter*/
	char name[DIRENT_MAXLEN+1];
	/* Numéro d'inode du dossier ou fichier auquel on va accéder maintenant */
	uint16_t child_inr;
	/* Nom du prochain sous-dossier ou fichier*/
	char* ptr=strchr(entry,'/');
	int l= strlen(entry);
	/*Nous ne somme pas encore arrivé au dossier ou fichier dont on cherche le numéro d'inode*/
	if(ptr!=NULL) l-=strlen(ptr);
	do{
		if ((err=direntv6_readdir(&d, name, &child_inr))<0) return err;
		/* Si nous somme déja dans le dossier ou fichier que l'on veut */
		if(strncmp(name,entry,strlen(entry))==0) return child_inr;
		/* Nous somme dans le dossier qui contient le sous-dossier ou fichier que l'on recherche */
		if(strncmp(name,entry,l)==0) {
			/* on modifie la taille qui devient la taille de notre nouvelle sequence à chercher */
			length=strlen(ptr);
			/* On cherche récursivement */
			return direntv6_dirlookup_core(u,child_inr,ptr,length);
		}
	}while(err>0);
	/* sinon une erreur s'est produite ou on cherche un fichier ou dossier qui n'existe pas */
	return ERR_INODE_OUTOF_RANGE;
}


/**
 * @brief get the inode number for the given path
 * @param u a mounted filesystem
 * @param inr the current of the subtree
 * @param entry the prefix to the subtree
 * @return inr on success; <0 on error
 */
int direntv6_dirlookup(const struct unix_filesystem *u, uint16_t inr, const char *entry)
{
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(entry);
    int num = direntv6_dirlookup_core(u, inr, entry, strlen(entry));
    return num;
}



/**
 * @brief create a new direntv6 with the given name and given mode
 * @param u a mounted filesystem
 * @param entry the path of the new entry
 * @param mode the mode of the new inode
 * @return inr on success; <0 on error
 */
 
int direntv6_create(struct unix_filesystem *u, const char *entry, uint16_t mode)
{
	int err=0;
	if((err=direntv6_dirlookup(u,ROOT_INUMBER,entry))>0) return ERR_FILENAME_ALREADY_EXISTS;
	
	/*pointeur vers le dernier dossier (celui que lon veut creer)*/
	char* ptr= strrchr(entry,'/');
	ptr++;
	
	size_t size_entry = strlen(entry);
	size_t taille_elem = strlen(ptr);
	if(taille_elem>DIRENT_MAXLEN) return ERR_FILENAME_TOO_LONG;
	size_t taille_repert = size_entry-taille_elem;
	
	/* Parent repository */
	char repertoire[taille_repert+1];
	strncpy(repertoire,entry,taille_repert);
	repertoire[taille_repert]='\0';	
	
	/* We get the inode number of the parent */
	int num_inode = direntv6_dirlookup(u, ROOT_INUMBER, repertoire);
	if(num_inode<=0) return ERR_BAD_PARAMETER;
	/* Now we can continue because the parent exists */
	int next = inode_alloc(u);
	if(next<0) return next;
	struct inode ino;
	memset(&ino,0,sizeof(ino));
	ino.i_mode = mode;
	if((err=inode_write(u,next,&ino))<0) return err;
	struct direntv6 dv6;
	memset(&dv6,0,sizeof(dv6));
	dv6.d_inumber= next;
	strncpy(dv6.d_name, ptr, taille_elem);
	struct filev6 fv6;
	//on lit le repertoire parent
	if ((err=filev6_open(u,num_inode,&fv6))<0) return err;
	if((err=filev6_writebytes(u, &fv6, &dv6, sizeof(struct direntv6)))<0) return err;
	
    return next;
}
