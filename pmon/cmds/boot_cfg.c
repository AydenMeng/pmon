/*
 * Copyright (c) 2007 SUNWAH HI-TECH  (www.sw-linux.com.cn)
 * Wing Sun	<chunyang.sun@sw-linux.com>
 * Weiping Zhu<weiping.zhu@sw-linux.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef __BOOT_CFG_C__
#define __BOOT_CFG_C__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/endian.h>
#include <sys/device.h>

#include <stdarg.h>
#include <termio.h>

#ifndef TCSASOFT
# define TCSASOFT 0
#endif

#ifdef _KERNEL
#undef _KERNEL
#include <sys/ioctl.h>
#define _KERNEL
#else
#include <sys/ioctl.h>
#endif

#ifdef USE_MD5_PASSWORDS
#include <md5.h>
#endif

#include <pmon.h>
#include <exec.h>
#include <pmon/loaders/loadfn.h>

#ifdef __mips__
#include <machine/cpu.h>
#endif

#include "boot_cfg.h"

Menu_Item *menus;//Storage All menu information.
MenuOptions menu_options[] = {
	{"showmenu", 0, 0, "1"}, //indentfy to show or not show menu for user.
	{"default", 0, 0, "0"}, //default menu to boot.
	{"timeout", 0, 0, "5"},//default timeout in seconds wait for user.
	{"password", 0, 1, ""},
	{"md5_enable", 0, 0, "0"},	/* 1-md5 */
};

int options_num = sizeof(menu_options) / sizeof(menu_options[0]);

/***************************************************
 * set a value of a option.
 **************************************************/
void set_option_value(const char* option, const char* value)
{
	int i = 0;

	for (i = 0; i < options_num; i++)
	{
		if (strcasecmp (option, menu_options[i].option) == 0)
		{
			strncpy (menu_options[i].value, value, GLOBAL_VALUE_LEN);
			menu_options[i].set_type = 1;
			return;
		}
	}
}
/***************************************************
 * Got a value of a option.
 **************************************************/
const char* get_option_value(const char* option)
{
	int i = 0;

	for (i = 0; i < options_num; i++)
	{
		if (strcasecmp(option, menu_options[i].option) == 0)
			return menu_options[i].value;
	}
	return NULL;
}

/**********************************************************
 * Delete SPACE and TAB from begin and end of the str.
 * Also delete \r\n at the end of the str.
 * Final check is the comment line.
**********************************************************/
char* trim(char *string)
{
  int len;
  char * str= string;

  len = strlen(str);
  while( len && (str[len-1] == '\n' || str[len-1] == '\r' ||
		  issep (str[len-1]) ) )
	  str[--len] = '\0';
  
  while( issep (*str) )
	  str++;
  
  if (*str == '#')
	  *str = '\0';
  
  return str;
}

/**********************************************************
 * Read a line from fd into buf.
 *********************************************************/
int ReadLine(ExecId id, int fd, char *buf, int buflen)
{
  int len, n;

  n = 0;
  len = buflen - 1;
  
  while( 1 )
  {
	  if( exec (id, fd, buf, &len, 0) <= 0 )//read data from fd.
		  break;//end of file or error.

	  n++;
	  
	  buf[len] = '\0';
	  while( len && (buf[len-1] == '\n' || buf[len-1] == '\r') )
	  {
		  buf[--len] = '\0';
	  }

	  if( len && buf[len-1] == '\\' )
	  {
		  //prepare to read another new data to buf.
		  buf += len - 1;
		  buflen -= len - 1;
		  if( !buflen )        /* No buffer space */
			  break;
	  }
	  else
		  break;
  }

  return n;
}

/*************************************************************
 * Remove comment information from string.
*************************************************************/

void remove_comment(char *str)
{
  //int instring;

  char *p;
  p = str;

//  instring = 0;
  while( *p )
  {
	switch( *p )
	{
		case '"':
		case '\'':
//			instring = !instring;
			break;
		case '#':
//			if( !instring )
				*p = '\0';
			return;
	}
	++p;
  }

}

/*********************************************************************
 * Split string to two part, string delimit by the SPACE or TAB.
 * char * str , strings need to be splited.
 * char * strings[2], buffer to storage splited strings.
*********************************************************************/

int split_str(const char * str,char * strings[2])
{
	char * p;
	char * p1;
	char * p2;

	//check input data buf.
	if( !str || !strings)
		return -1 ;
	if( !strings[0] || !strings [1] )
		return -1;

	//Remove SPACE or TAB from header and tail.
	p = trim(str);
	//if is empty line or comment data.
	if( *p == '\0' || *p == '#')
		return -1;
	//found where is the SPACE or TAB.
	p1 = index (str, ' ');
	p2 = index (str, '\t');

	//Get the first position.
	if( p1 < p2 )
		p = p1 ? p1:p2;
	else
		p = p2 ? p2:p1;
	
	if( p )// if found copy data.
	{
		strncpy (strings[0], str, p - str);
		strcpy (strings[1], p);
		return 2;
	}
	else
		strcpy (strings[0], str);
	return 1;
}
//split string by char
int split_bychar(const char * str, const char c, char * strings[2])
{
	char * p;

	//check input data buf.
	if( !str || !strings)
		return -1 ;
	if( !strings[0] || !strings [1] )
		return -1;

	//Remove SPACE or TAB from header and tail.
	p = trim(str);
	//if is empty line or comment data.
	if( *p == '\0' || *p == '#')
		return -1;
	if (*p == c)
		++p;
	//found where is the SPACE or TAB.
	p = index (str, c);

	if( p )// if found copy data.
	{
		strncpy (strings[0], str, p - str);
		strcpy (strings[1], p+1);
		return 2;
	}
	else
		strcpy (strings[0], str);
	return 1;
}
/*********************************************************************
 * Split string to key and value.
 * char * str, string to be splited
 * char * option, buffer to stor key.
 * int option_len, length of the buffer option.
 * char * value, buffer to stor value.
 * int value_len, length of the buffer value.
 * return -1 for failed, 0 for success, 1 for grub
*********************************************************************/

int GetOption (char *str, char *option, int option_len, char *value,
					  int val_len, char *other)
{
	char *argv[2];
	char key [MENU_TITLE_BUF_LEN +1] = {0};
	char val [VALUE_LEN +1] = {0};
	char val_added [VALUE_LEN +1] = {0};
	char *p;
	int argc;

	argv[0] = key;
	argv[1] = val;

	//split string to two part,key and value ,splited by SPACE or TAB
	argc = split_str (str, argv);


	//if split to two part, return 2.
	if( argc < 2 )
		return -1;
	if (argc == 2) {
		//remove SPACE and TAB from key and val header and tail.
		p = trim (key);
		if( strcasecmp (p, "set") == 0 ) {
			p = trim(val);
			remove_comment(p);
			argc = split_bychar(p, '=', argv);
			if (argc == 2) {
				p = trim (key);
				strncpy (option, p, option_len - 1);
				if (strcasecmp(p, "root") == 0) {
					p = trim (val);
					memset(key, 0, MENU_TITLE_BUF_LEN);
					while (*p) {
						if (*p >= '0' && *p <= '9') {
							if (*(p+1) == ',') {
								sprintf(key, "/dev/sd%c", (9 - ('9' - *p)) + 'a');
								sprintf(val_added, "wd%d", *p - '0');
							} else {
								sprintf(val, "%s%c", key, *p);
								sprintf(other, "%s%c", val_added, (8 - ('9' - *p)) + 'a');
							}
						}
						++p;
					}
					strncpy (value, val, val_len -1);
					return 1;
				}
			}
		}
		//copy to buffer.
		strncpy (option, p, option_len - 1);

		p = trim (val);
		//remove comment data from string.
		remove_comment(p);
		argc = split_str(p, argv);
		if (argc < 1)
			return -1;
		if (argc == 1) {
			strncpy (value, p, val_len -1);
			return 0;
		} else {
			//remove SPACE and TAB from key and val header and tail.
			p = trim (key);
			remove_comment(p);
			//copy to buffer.
			strncpy (value, p, val_len -1);
			//args...
			p = trim (val);
			remove_comment(p);
			strncpy (other, p, val_len -1);

			return 1;
		}
	}

	return -1;
}

/******************************************************************
 * Extract the menu title from string.
 * const char * str, the string include menu title information.
 * if found, return 0, else return -1.
******************************************************************/

int GetTitle(const char *str,char * title, int title_buf_len)
{
	char *argv[2];
	char key [MENU_TITLE_BUF_LEN +1] = {0};
	char value [VALUE_LEN +1] = {0};
	char *p;
	int argc, len;
  
	argv[0] = key;
	argv[1] = value;

	//Parse string.
	argc = split_str (str, argv);

	if( argc < 1 )
		return -1;

	if( argc == 1 )
	{
		//Support for OLD style lable
		len = strlen (argv[0]);
		if( argv[0][len -1] == ':')
		{
			argv[0][len -1] = '\0';
			p=trim(argv[0]);
			//remove SPACE and TAB from begin and end of string.
			strncpy(title,p,title_buf_len);
			return 0;
		}
	}
	else
	{
		//remove SPACE and TAB from begin and end of string.
		p = trim (key);
		//check key.
		if( strcasecmp (p, "title") == 0 )
		{
			//remove SPACE and TAB from begin and end of string.
			p = trim (value);
			//remove comment data from string.
			remove_comment(p);
			strncpy (title, p, title_buf_len);
			return 0;
		}
		//deal with grub.cfg
		if( strcasecmp (p, "menuentry") == 0 )
		{
			//remove SPACE and TAB from begin and end of string.
			p = trim (value);
			//Parse string.
			argv[0] = index(p, '\'');
			p = argv[0] + 1;
			argv[1] = index(p, '\'');
			strncpy(title, p, argv[1] - p);
			return 0;
		}
		//deal with submenu in grub.cfg
		if( strcasecmp (p, "submenu") == 0 )
		{
			//remove SPACE and TAB from begin and end of string.
			p = trim (value);
			//Parse string.
			argv[0] = index(p, '\'');
			p = argv[0] + 1;
			argv[1] = index(p, '\'');
			strncpy(title, p, argv[1] - p);
			return 1;
		}
	}
	return -1;
}

void assign_menu(char *cp, char *option, char *value, char *other, Menu_Item *menu, char *root_prefix)
{
	int n = 0;
	DeviceDisk *d;
	//Read all properties of current menu item.
	if( GetOption (cp,option,OPTION_LEN,value,VALUE_LEN, other) == 0 )
	{
		if( strcasecmp (option,"kernel") == 0 || strcasecmp (option,"linux") == 0 ) // got kernel property
		{
			if( menu->kernel != NULL && menu->kernel[0] == '\0') // we only sotr the firs kernel property, drop others.
				strncpy(menu->kernel,value,VALUE_LEN);
		}
		else if( strcasecmp (option,"args") == 0 )// got kernel arguments property.
		{
			if( menu->args != NULL && menu->args[0] == '\0')//same as the kernel property.
				strncpy(menu->args,value,VALUE_LEN);
		}
		else if( strcasecmp (option,"initrd") == 0 )// go initrd kernel arguments property, may be null.
		{
			memset(option, 0, OPTION_LEN);
			sprintf(option, "%s%s", root_prefix, value);
			if( menu->initrd!= NULL && menu->initrd[0] == '\0')// same as the kernel property.
				strncpy(menu->initrd, option, VALUE_LEN);
		}
		else if (strcasecmp(option, "root") == 0)
		{
			if (menu->root != NULL && menu->root[0] == '\0')
			{
				strncpy(menu->root, value, VALUE_LEN);
			}
		}
	} else if (GetOption (cp,option,OPTION_LEN,value,VALUE_LEN, other) == 1) {
		if( strcasecmp (option,"kernel") == 0 || strcasecmp (option,"linux") == 0 ) // got kernel property
		{
			if( menu->kernel != NULL && menu->kernel[0] == '\0') // we only sotr the firs kernel property, drop others.
			{
				memset(option, 0, OPTION_LEN);
				sprintf(option, "%s%s", root_prefix, value);
				strncpy(menu->kernel, option, VALUE_LEN);
			}
			if( menu->args != NULL && menu->args[0] == '\0')//same as the kernel property.
			{
				strncpy(menu->args, other, VALUE_LEN);
			}
		}
		else if (strcasecmp(option, "root") == 0)
		{
			strncpy(root_prefix, other, VALUE_LEN);
			n = root_prefix[strlen(root_prefix)-1] - 'a';
			d = FindDevice(root_prefix);
			if (d == NULL) {
				return (-1);
			}
			sprintf(root_prefix, "/dev/fs/%s@%s", d->part[n]->fs->fsname, other);
			if (menu->root != NULL && menu->root[0] == '\0')
			{
				strncpy(menu->root, value, VALUE_LEN);
			}
		}
	}
}

void set_menuitem(Menu_Item * menu)
{
	memset (menu->kernel, 0, VALUE_LEN + 1);
	memset (menu->args, 0, VALUE_LEN + 1);
	memset (menu->initrd, 0, VALUE_LEN + 1);
	memset (menu->root, 0, VALUE_LEN + 1);
	menu->Next = NULL;
	menu->Sub = NULL;
}

/*************************************************************************
 * Read menu config file and initial data struct to storage information.
*************************************************************************/

int menu_list_read(ExecId id, int fd, int flags)
{
	int menus_num = 0;
	int i, j;//index for current menu item.
	char buf[1025];
	char buf_bak[1025];
	int buflen = 1024;
	int n = 0;
	char* cp = NULL;
	char option[OPTION_LEN] = {0};
	char value[VALUE_LEN] = {0};
	char other[VALUE_LEN] = {0};
	char root_prefix[VALUE_LEN] = {0};
	int in_menu = 0;
	int issubmenu = 0;
	char title [MENU_TITLE_BUF_LEN + 1];//Title of menu item, display on screen.
	DeviceDisk *d;
	
	Menu_Item *curr, *next, *sub, *tmp;
	menus = (Menu_Item *)malloc(sizeof(Menu_Item));
	tmp = NULL;
	next = NULL;
	sub = NULL;
	curr = menus;
	set_menuitem(curr);
	
	j = -1; //set to 0;
	n = 0;
	i = 0;

	while (ReadLine(id, fd, buf, buflen) > 0)//Read a line.
	{
		memset(title,0, MENU_TITLE_BUF_LEN + 1 );

		strcpy(buf_bak, buf);//Got a copy of buf.
		cp = trim(buf);//Trim space
		if (*cp == '\0' || *cp == '#')//If only a empty line,or comment line, drop it.
		{

			continue;
		}

		if (cp[strlen(cp)-1] == '{')
			i++;
		else if (cp[strlen(cp)-1] == '}')
			i--;

		//Check data, looking for menu title.
		if( GetTitle (cp, title, MENU_TITLE_BUF_LEN ) == 0 && i % 2 == 1)
		{
			next = (Menu_Item *)malloc(sizeof(Menu_Item));
			if (tmp != NULL) {
				tmp->Next = next;
				tmp = NULL;
			} else {
				curr->Next = next;
			}
				curr = next;
			strncpy (next->title, title, MENU_TITLE_BUF_LEN);//storage it.
			set_menuitem(next);
			j++;
			in_menu = 1;
			issubmenu = 0;
			continue;
		}
		//deal with submenu in grub.cfg
		if( GetTitle (cp, title, MENU_TITLE_BUF_LEN ) == 1 && i % 2 == 1)
		{
			next = (Menu_Item *)malloc(sizeof(Menu_Item));
			curr->Next = next;
			curr = next;
			tmp = next;
			strncpy (next->title, title, MENU_TITLE_BUF_LEN);//storage it.
			set_menuitem(next);
			sub = (Menu_Item *)malloc(sizeof(Menu_Item));
			curr->Sub = sub;
			curr = sub;
			set_menuitem(sub);
			j++;
			issubmenu = 1;
			in_menu = 0;
			continue;
		}

		cp = trim(buf_bak);
		if (issubmenu) {
			if( GetTitle (cp, title, MENU_TITLE_BUF_LEN ) == 0 && i % 2 == 0)  {
				next = (Menu_Item *)malloc(sizeof(Menu_Item));
				curr->Next = next;
				curr = next;
				strncpy (next->title, title, MENU_TITLE_BUF_LEN);//storage it.
				set_menuitem(next);
			}
			assign_menu(cp,option,value,other,next, root_prefix);
		}

		if( in_menu )
		{
			assign_menu(cp,option,value,other,next, root_prefix);
		}
		else // out of menu item.
		{
			//Check data, looking for global option.
			if( GetOption (cp,option,OPTION_LEN,value,VALUE_LEN, other) == 0 )
			{
				set_option_value(option, value);//storage it.
			}
		}
	}
	menus_num = j + 1;
	return menus_num > 0 ? 0 : -1;
}

/************************************************
 * try to load a boot configure file.
 * const char * filename the file to be loaded.
 * return -1 for load failed.
 * return 0 for successful.
 ***********************************************/

int OpenLoadConfig(const char* filename)
{
	int bootfd;
	if ((bootfd = open(filename, O_RDONLY | O_NONBLOCK)) < 0) {
		return -1;
	}
	return bootfd;
}

/**********************************************************
 * Execute menu item
 * int index , the index of menu_items[] to execute.
 * return -2 means index great than all menu item in array menu_items.
 * retunr -1 means missing some parameter of current menu item, or 
 * parameter of current menu item was incorrect.
 * return 0 means successfully execute the menu item.
 *********************************************************/
int load_kernel_from_menu(Menu_Item* pItem)
{
	char cmd[1025];
	int is_root = 0;
	int stat;

	if (pItem == NULL)
	{
		return -1;
	}
	
	if (pItem->kernel == NULL || pItem->kernel[0] == '\0')
	{
		return -1;
	}
	
	if (pItem != NULL && pItem->root[0] != '\0')
	{
		is_root = 1;
	}
		
	printf("Now booting the %s\n", pItem->title);
	if(pItem->kernel[0] != '\0')
	{
		memset(cmd, 0, sizeof(cmd));
		strncpy(cmd, pItem->kernel, 5);
#if 0
		if (is_root && strcasecmp(cmd, "/dev/") != 0 && cmd[0] != '(')
		{
			sprintf(cmd, "load %s/%s", pItem->root, pItem->kernel);
		}
		else
		{
			sprintf(cmd, "load %s", pItem->kernel);
		}
#endif
		if (is_root && strcasecmp(cmd, "/dev/") != 0 && cmd[0] != '(')
		{
			sprintf(cmd, "%s/%s", pItem->root, pItem->kernel);
		}
		else
		{
			strcpy(cmd, pItem->kernel);
		}

#ifdef MENU_DEBUG
		printf("%s\n",cmd);
#endif
//		stat=do_cmd(cmd);
		stat = boot_kernel(cmd, 0, NULL, 0);
#ifdef MENU_DEBUG
		printf("Load Kernel return %d\n",stat);
#endif
		if(stat)
			return -1;
	}
	else
	{
		printf("No kernel to load for current menu item.\n");
		return -1;
	}
	
	return 0;
}

int load_initrd_from_menu(Menu_Item* pItem)
{
	char cmd[1025];
	int is_root = 0;
	int stat;

	if (pItem == NULL)
	{
		return -1;
	}
	
	if (pItem->root != NULL && pItem->root[0] != '\0')
	{
		is_root = 1;
	}
		
	if(pItem->initrd[0] != '\0')
	{
		memset(cmd, 0, sizeof(cmd));
		strncpy(cmd, pItem->initrd, 5);
#if 0
		if (is_root && strcasecmp(cmd, "/dev/") != 0 && cmd[0] != '(')
		{
			sprintf(cmd, "initrd %s/%s", pItem->root, pItem->initrd);
		}
		else
		{
			sprintf(cmd, "initrd %s", pItem->initrd);
		}
#endif
		if (is_root && strcasecmp(cmd, "/dev/") != 0 && cmd[0] != '(')
		{
			sprintf(cmd, "%s/%s", pItem->root, pItem->initrd);
		}
		else
		{
			strcpy(cmd, pItem->initrd);
		}

#ifdef MENU_DEBUG
		printf("%s\n",cmd);
#endif
//		stat=do_cmd(cmd);
		stat = boot_initrd(cmd, 0x84000000,0);
#ifdef MENU_DEBUG
		printf("Load initrd return %d\n",stat);
#endif
		if(stat)
			return -1;
	}

	return 0;
}

int boot_run_from_menu(Menu_Item* pItem)
{
	char cmd[1025];

//#define MENU_DEBUG
#ifdef MENU_DEBUG
	int stat;
#endif

	if (pItem == NULL)
	{
		return -1;
	}
	
	if(pItem->args[0] != '\0')
	{
#ifdef NOTEBOOK
		sprintf(cmd, "g -S %s", pItem->args);
#else
		sprintf(cmd,"g %s", pItem->args);
#endif
		
#ifdef MENU_DEBUG
		printf("%s\n",cmd);
		stat=do_cmd(cmd);
		printf("go command return %d\n",stat);
		if(stat)
#else
		printf("Boot with parameters: %s\n", pItem->args);
		if(do_cmd(cmd))
#endif
			return -1;
	}
	else
	{
		printf("No arguments pass to kernel in current menu item.\n");
		return -1;
	}
	return 0;
}

int boot_load_from_menu(Menu_Item* pItem)
{
	int ret;

	char buf[LINESZ];
	if(getenv("autocmd"))
	{
		strcpy(buf,getenv("autocmd"));
		do_cmd(buf);
	}

	ret = load_kernel_from_menu(pItem);
	if (ret != 0)
	{
		return -1;
	}

	ret = load_initrd_from_menu(pItem);
	if (ret != 0)
	{
		return -1;
	}
	
	ret = boot_run_from_menu(pItem);
	if (ret != 0)
	{
		return -1;
	}

	return 0;
}

Menu_Item * locate_menu(Menu_Item *head, int index)
{
	if (head == NULL)
		return NULL;
	Menu_Item *p = (head)->Next;
	int i;
	if (index < 0 || p == NULL)
		return NULL;
	for (i = 0; i < index && p != NULL; i++) {
		p = p->Next;
	}
	return p;
}

void free_menu(Menu_Item *head)
{
	if (head == NULL)
		return NULL;
	Menu_Item *p;
	while (head != NULL) {
		p = head->Next;
		free(head);
		head = p;
	}
}

int get_menu_nums(Menu_Item *head)
{
	int menus_num = 0;
	if (head == NULL)
		return 0;
	Menu_Item *p = (head)->Next;
	int i;
	if (p == NULL)
		return 0;
	for (menus_num = 0; p != NULL; menus_num++) {
		p = p->Next;
	}
	return menus_num;
}

int boot_load(Menu_Item *head, int index)
{
	char cmd[1025];
	int is_root = 0;
	Menu_Item *p = locate_menu(head, index);
//#define MENU_DEBUG
#ifdef MENU_DEBUG
	int stat;
#endif
	
	return boot_load_from_menu(p);
	
#if 0
	if( menu_items[index].kernel == NULL )
		return -1;
	
	if (menu_items[index].root[0] != '\0')
	{
		is_root = 1;
	}
		
	printf("Now booting the %s\n",menu_items[index].title);
	if(menu_items[index].kernel[0] != '\0')
	{
		memset(cmd, 0, sizeof(cmd));
		strncpy(cmd, menu_items[index].kernel, 5);
		if (is_root && strcasecmp(cmd, "/dev/") != 0 &&
			cmd[0] != '(')
		{
			sprintf(cmd, "load %s/%s", menu_items[index].root, menu_items[index].kernel);
		}
		else
		{
			sprintf(cmd, "load %s", menu_items[index].kernel);
		}

#ifdef MENU_DEBUG
		printf("%s\n",cmd);
		stat=do_cmd(cmd);
		printf("Load Kernel return %d\n",stat);
		if(stat)
#else
		if(do_cmd(cmd))
#endif
			return -1;
	}
	else
	{
		printf("No kernel to load for current menu item.\n");
		return -1;
	}
	
	if(menu_items[index].initrd[0] != '\0')
	{
		memset(cmd, 0, sizeof(cmd));
		strncpy(cmd, menu_items[index].initrd, 5);
		if (is_root && strcasecmp(cmd, "/dev/") != 0 &&
			cmd[0] != '(')
		{
			sprintf(cmd, "initrd %s/%s", menu_items[index].root, menu_items[index].initrd);
		}
		else
		{
			sprintf(cmd, "initrd %s", menu_items[index].initrd);
		}

#ifdef MENU_DEBUG
		printf("%s\n",cmd);
		stat=do_cmd(cmd);
		printf("Load initrd return %d\n",stat);
		if(stat)
#else
		if(do_cmd(cmd))
#endif
			return -1;
	}

	if(menu_items[index].args[0] != '\0')
	{
#ifdef NOTEBOOK
		sprintf(cmd, "g -S %s", menu_items[index].args);
#else
		sprintf(cmd,"g %s",menu_items[index].args);
#endif
		
#ifdef MENU_DEBUG
		printf("%s\n",cmd);
		stat=do_cmd(cmd);
		printf("go command return %d\n",stat);
		if(stat)
#else
		printf("Boot with parameters: %s\n",menu_items[index].args);
		if(do_cmd(cmd))
#endif
			return -1;
	}
	else
	{
		printf("No arguments pass to kernel in current menu item.\n");
		return -1;
	}
	return 0;
#endif
}
#endif

int load_list_menu(const char* path)
{
	int flags = 0;
	int bootfd;
	ExecId id;
	int ret;
	int boot_id;

	bootfd = OpenLoadConfig(path);
	if (bootfd == -1)
	{
		return -1;
	}

	id = getExec("txt");
	if (id != NULL) {
		ret = menu_list_read(id, bootfd, flags);
		if (ret != 0)
		{
			printf("\nCannot found and boot item in boot configure file.");
			printf("\nPress any key to continue ...\n");
			getchar();
			close(bootfd);
			return -2;
		}
	}else{
		printf("[error] this pmon can't read file!");
		close(bootfd);
		return -3;
	}
	close(bootfd);

	return 0;
}

int do_cmd_boot_load(Menu_Item *head, int boot_id, int device_flag)
{
	int ret = -1;
	struct termio sav;
	int menus_num = get_menu_nums(head);

#if 0
	if (boot_id == -1 && check_cdrom() && device_flag == IDE)
    {
        ioctl (STDIN, CBREAK, &sav);
        ret=do_cmd_menu_list(CDROM, "/dev/iso9660/usb0");
        ioctl (STDIN, TCSETAF, &sav);
    }
    else if (boot_id == -1 && check_ide() && device_flag == CDROM)
    {
        ioctl(STDIN, CBREAK, &sav);
        ret = do_cmd_menu_list(IDE, "(wd0,0)");
        ioctl(STDIN, TCSETAF, &sav);
    }
	else
#endif
	{
		//printf("do_cmd_menu_list\n");

		if (boot_id < menus_num)
		{
			ret = boot_load(head, boot_id);
			if (ret <0 )
			{
				printf("\nConfiguration failed.\nPress any key to continue ...%d\n", ret);
				getchar();
			}
			return ret;
		}
	}

	return ret;
}

int check_config (const char * file)
{
	int bootfd;
	bootfd = OpenLoadConfig (file);
	if( bootfd == -1 )
		return 0;
	close(bootfd);
	return 1;
}

