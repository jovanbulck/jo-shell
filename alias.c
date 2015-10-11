/* This file is part of jsh.
 * 
 * jsh: A basic UNIX shell implementation in C
 * Copyright (C) 2014 Jo Van Bulck <jo.vanbulck@student.kuleuven.be>
 *
 * jsh is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * jsh is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with jsh.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "alias.h"

#define MAX_ALIAS_VAL_LENGTH    200 // the maximum allowed number of chars per alias value
#define MAX_ALIAS_KEY_LENGTH    50  // the maximum allowed number of chars per alias key

struct alias {
    char *key;
    char *value;
    struct alias *next;
};
//typedef struct alias ALIAS;
//#define CREATE_ALIAS(k,v) (struct alias) {k, v}

struct alias *head = NULL;
struct alias *tail = NULL;

int total_alias_val_length = 0;
int nb_aliases = 0;

bool alias_key_changed = false;

/*
 * alias: create a mapping between a key and value pair that can be resolved with resolvealiases().
 *  returns EXIT_SUCCESS or EXIT_FAILURE if something went wrong (e.g. malloc)
 *  (note that provided strings that are too long are silently truncated)
 */
int alias(char *k, char *v) {
    // allow recursive alias definitions
    char *val = resolvealiases(v);

    int vallength = strnlen(val, MAX_ALIAS_VAL_LENGTH);
    int keylength = strnlen(k, MAX_ALIAS_KEY_LENGTH);
    
    // alloc memory for the new alias struct and its key
    // note: *val has already been alloced by the resolvealiases() call
    struct alias *new = malloc(sizeof(struct alias)); //TODO chkerr
    new->next = NULL;
    new->key = malloc(sizeof (char) * keylength+1);
    
    // copy the provided strings into the allocated memory
    strncpy(new->key, k, keylength+1);
    new->value = val;
    
    if(alias_exists(k)) {
        unalias(k);
    }
	total_alias_val_length += vallength;

    if (head == NULL) {
	    head = new;
	    tail = head;
	}
	else {
	    tail->next = new;
	    tail = new;
	}
	nb_aliases++;
	alias_key_changed = true;
    return EXIT_SUCCESS;
}

/*
 * unalias: unaliases a specified key
 *  returns EXIT_SUCCESS if the specified key was found; else prints an error message and returns EXIT_FAILURE 
 */
int unalias(char *key) {
    struct alias *cur = head;
    struct alias *prev = NULL;
    while (cur != NULL) {
        if (strncmp(cur->key, key, MAX_ALIAS_KEY_LENGTH) == 0) {
            // re-organize the linked list
            if (prev)
                prev->next = cur->next;
            else
                head = cur->next;
            if (cur == tail)
                tail = prev;
            // free the unalised alias and return
            total_alias_val_length -= strnlen(cur->value, MAX_ALIAS_VAL_LENGTH);
            free(cur);
            nb_aliases--;
            alias_key_changed = true;
            return EXIT_SUCCESS;;
        }
        prev = cur;
        cur = cur->next;
    }
    printerr("unalias: no such alias key: %s", key);
    return EXIT_FAILURE;
}

/*
 * printaliases: print a list of all currently set aliases on stdout
 * returns EXIT_SUCCESS
 */
int printaliases() {
    struct alias *cur = head;
    while(cur != NULL) {
        printf("alias %s = '%s'\n", cur->key, cur->value);
        cur = cur->next;
    }
    return EXIT_SUCCESS;
}

/*
 * get_all_alias_keys: returns a newly malloced array of pointers to newly malloced strings
 *  containing a copy of the alias keys.
 * @param nb_keys           : if non-NULL; will contain the length of the result array;
 *  nb_keys will be untouched if NULL is returned
 * @param only_on_change    : if true, the result will only be non-NULL if one of the alias
 *  keys changed since the last time this method was called.
 * @return: an array with all alias key strings, or NULL iff @param(only_on_change) and
 *  there have been no alias key changes since the last time this method was called.
 * @note: the caller is responisble for freeing the array as well as the array elements
 */
char **get_all_alias_keys(unsigned int *nb_keys, bool only_on_change) {
    if (only_on_change && !alias_key_changed) {
        return NULL;
    }
    
    char **ret = malloc(sizeof(char*) * nb_aliases);

    int i = 0;
    struct alias *cur = head;
    while(cur != NULL) {
        ret[i] = strclone(cur->key);
        cur = cur->next;
        i++;
    }
    if (nb_keys) *nb_keys = nb_aliases;
    alias_key_changed = false;
    return ret;
}


/*
 * resolvealiases: substitutes all known aliases in the inputstring. Returns a pointer to the alias-expanded string.
 *  NOTE: this function returns a pointer to a newly malloced() string. The caller should free() it afterwards, 
 *        as well as also the inputstring *s, if needed
 *
 *  current limitations for aliases:
 * TODO - any spaces in the value must be escaped in the input for the 'alias' cmd    e.g. alias ls ls\ --color=auto
 *                                                                                    alt syntax: alias ls "ls --color=auto"
 */
char *resolvealiases(char *s) {
    bool is_valid_alias(char*, char*, int); // helper function def

    // alloc enough space for the return value
    int maxsize = strlen(s) + total_alias_val_length; 
    char *ret = malloc(sizeof (char) * maxsize);
    strcpy(ret, s);
    
    // find all alias key substrings, replacing them if valid in context
    struct alias *cur;
    char *p, *str; // p is pointer to matched substring; str is pointer to not-yet-checked string
    for (cur = head; cur != NULL; cur = cur->next)
        for (str = ret; (p = strstr(str, cur->key)) != NULL;) {
            if (is_valid_alias(cur->key, ret, p-ret)) {
                printdebug("alias: '%s' VALID in context '%s'", cur->key, p);
                
                char *after = p + strlen(cur->key);
                memmove(p + strlen(cur->value), after, strlen(after)+1); // overlapping mem; len+1 : also copy the '\0'
                memcpy(p, cur->value, strlen(cur->value)); // non-overlapping mem
                str = p+strlen(cur->value);
                }
            else {
                printdebug("alias: '%s' INVALID in context '%s'", cur->key, p);
                str = p+strlen(cur->key);
            }
        }
    
    printdebug("alias: input resolved to: '%s'", ret);
    return ret;
}

/* 
 * is_valid_alias: returns whether or not an occurence of an alias key is valid (i.e. must be 
 *  replaced by its value) in a given context string. An alias match is valid iff it occurs 
 *  as a comd in the grammar and it's not '\' escaped.
 *
 *  @param key: the alias key corresponding to the alias that is matched
 *  @param context: the context string where the alias is matched
 *  @param i: the index in the context string where the alias is matched: context+i equals alias->key
 */
bool is_valid_alias(char *key, char *context, int i) {
    // allow escaping '\' of aliases TODO is this usefull? --> yes for the ~ alias hack
    if ( i > 0 && *(context+i-1) == '\\' ) {
        printdebug("alias: escaping '%s'", key);
        memmove(context+i-1, context+i, strlen(context+i)+1); //TODO this function shouldn't change the given context
        return false;
    }
    
    // built_in aliases are valid in any context TODO unless escaped...
    if (*key == '~')
        return true;
    
    return is_valid_cmd(key, context, i);
}

/*
 * alias_exists: returns whether or not a specified key is currently aliased.
 * @return true if the supplied alias already exists; else false.
 */
bool alias_exists(char* key) {
	if (!head) return false;

	struct alias *curAlias;
    for (curAlias = head; curAlias != NULL; curAlias = curAlias->next) {
		// If the current alias matches the argument, return true.
		if (strcmp(curAlias->key, key) == 0) return true;
	}
	return false;
}
