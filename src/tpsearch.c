/* search files related to print  Copyright (c) 2012 Jeffrin Jose

This   program  is   free  software:   you  can
redistribute  it  and/or  modify it  under  the
terms  of  the GNU  General  Public License  as
published  by  the  Free  Software  Foundation;
either version  3 of  the License, or  (at your
option) any later version.

This program is distributed in the hope that it
will  be  useful,  but  WITHOUT  ANY  WARRANTY;
without   even    the   implied   warranty   of
MERCHANTABILITY  or  FITNESS  FOR A  PARTICULAR
PURPOSE. See the GNU General Public License for
more details.

You  should have  received  a copy  of the  GNU
General   Public   License   along  with   this
program. If not, see http://www.gnu.org/licenses/.


     You can contact the author (Jeffrin Jose) by sending a mail
     to ahiliation@yahoo.co.in
*/

#include<stdio.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<pwd.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<dirent.h>
#include<stdbool.h>


int main(int argc, char *argv[]) {
int i =0;
char *p;
FILE *sfile;
char buffer[200];
char *sword = "file.c"; /* testing */
bool found = false;
struct passwd *pw = getpwuid(getuid());

char *homedir = pw->pw_dir;
const char *plocation = "/.trueprint/files";
/* printf("%s\n",homedir); */
const char *npath = strcat(homedir,plocation);
int value = chdir(npath);
DIR *opendirp = opendir(npath);
struct dirent *d; 
while ((d = readdir(opendirp)) != NULL )
/* for (i=0;i<=5;i++) */
{
sfile=fopen(d->d_name,"r");
while (!found && (fgets(buffer, sizeof(buffer), sfile ) != 0))
{
found = strstr(buffer,sword);
}
/* printf("%d\n",found); */
if (found == 1)
printf("found in: %s\n",d->d_name); 
fclose(sfile);

/* printf("%d %d\n",value,errno); */
/* printf("%s\n",d->d_name); */
} 
closedir(opendirp);

return 0;
}

