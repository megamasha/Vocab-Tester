#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#ifdef _WIN32
# define CLEARCOMMAND "cls"
#elif defined __unix__
# define CLEARCOMMAND "clear"
#else
# error "Could not detect OS. Clear screen may not work."
# define CLEARCOMMAND "cls"
#endif 

#define DINPUTFILENAME "vtdb.~sv"
#define DOUTPUTFILENAME "vtdb.~sv"
#define MAXINTVALUE 2147483647
#define MAXTEXTLENGTH 256
#define N2LTONORM 5
#define NORMTON2L 3
#define NORMTOKNOWN 5
#define KNOWNTONORM 2
#define KNOWNTOOLD 2
#define OLDTONORM 1

struct vocab
{
    int index; //identifies the entry in the list, allowing it to be selected by use of a random number
    char * question;//pointer to question text
    char * answer;//pointer to the answer text, which is required for the response to be considered correct
    char * info;//pointer to optional extra text giving advice such as to how to format the response
    char * hint;//pointer to optional text giving a clue to the answer
    int right;//indicates whether counter is counting correct or incorrect responses
    int counter;//counts how many times in a row the answer has been correct/incorrect
    int known;//indicates to what level the vocab is known, and thus to which list it belongs (when loading/saving)
    struct vocab * next;//pointer to next in list
};

struct listinfo//struct holds head, tail and the number of entries for the n2l, norm, known and old lists
{
    struct vocab * head;
    int entries;
    struct vocab * tail;
};

struct listinfo n2l, norm, known, old;
int n2l_flag; //Prevents 'need to learn's coming up twice in a row
int maxtextlength = MAXTEXTLENGTH; //allows use of this #define within text strings
FILE * inputfile;
FILE * outputfile;

void getrecordsfromfile(char * inputfilename,char separator);//load
char * readtextfromfile(int maxchars,char separator);//get text field from file
int readnumberfromfile(int maxvalue,char separator);//get integer field from file
struct vocab * addtolist(struct vocab * newentry, struct listinfo * list);//add given (already filled in) vocab record to given list
int removefromlist(struct vocab * entry, struct listinfo * list,int freeup);//remove given entry from given list. Also destroy record if freeup is true
void reindex (struct listinfo * list);//necessary to stop gaps in the numbering system, which could cause random vocab selection to fail
int writeliststofile();//save
void testme();//main code for learning vocab, including options menu
char * gettextfromkeyboard(char * target,int maxchars);//set given string (char pointer) from keyboard, allocating memory if necessary
int getyesorno();//asks for yes or no, returns true (1) if yes
void wank();//prints a random number from 1-12... obviously :-p
void testrandom();//code keeps causing exceptions (now fixed), and as it's so random, I'm guessing it's to do with the random numbers
void clrscr();//clears the screen. Now with #ifdef preprocessor script for portability!! 
void clearinputbuffer();//clears the input buffer after each request for input, so that the following request is not getting the overflow

void getrecordsfromfile(char * inputfilename,char separator)
{
    int counter = 0;
    struct vocab * newvocab;
    struct listinfo * newvocablist;
    if (!(inputfile = fopen(inputfilename, "r")))
    {
        printf("Unable to read input file. File does not exist or is in use.\n");
    }    
    else
    {
        printf("Opened input file %s, reading contents...\n",inputfilename);
        while (!feof(inputfile))
        {
            newvocab = (struct vocab *)malloc(sizeof(struct vocab));
            if (!newvocab)
            {
                printf("Memory allocation failed!\n");
                return;
            }
            else
            {
                newvocab->question=readtextfromfile(MAXTEXTLENGTH,separator);
                newvocab->answer=readtextfromfile(MAXTEXTLENGTH,separator);
                newvocab->info=readtextfromfile(MAXTEXTLENGTH,separator);
                newvocab->hint=readtextfromfile(MAXTEXTLENGTH,separator);
                newvocab->right=readnumberfromfile(1,separator);
                newvocab->counter=readnumberfromfile(0,separator);
                newvocab->known=readnumberfromfile(3,separator);

                switch (newvocab->known)
                {
                    case 0: newvocablist = &n2l;break;
                    case 1: newvocablist = &norm;break;
                    case 2: newvocablist = &known;break;
                    case 3: newvocablist = &old;break;
                }

                addtolist(newvocab,newvocablist);
                if (newvocab->question==NULL||newvocab->answer==NULL)
                {
                    printf("Removing empty vocab record created from faulty input file...\n");
                    removefromlist(newvocab,newvocablist,1);
                }
                else counter++;
            }
        }
        fclose(inputfile);
        printf("...finished.\n%i entries read from %s.\n\n",counter,inputfilename);
    }
    return;
}

char * readtextfromfile(int maxchars,char separator)
{
    int i=0;
    char ch;
    char * target = (char *)malloc(maxchars+1); //allocate memory for new string
    if (!target) {printf("Memory allocation failed!\n");return 0;}//return 0 and print error if alloc failed

    ch=getc(inputfile);
    if (ch==separator||ch==EOF){free(target);return NULL;}//if field is blank (zero-length), return null pointer (||EOF added because it hangs on blank database)
    while (isspace(ch))
    {
        ch = getc(inputfile);//cycle forward until you reach text
        if (ch == separator||ch=='\n'||ch==EOF) {free(target);return NULL;}//if no text found(reached ~ before anything else), return null pointer
    }
    if (ch=='"') //Entry is in quotes (generated by excel when exporting to .csv and field contains a comma)
    {
        ch=getc(inputfile);//move to next character after the quotes
        while (i<maxchars && ch!='"' && ch!='\n')//stop when you reach the end quotes, end of line, or when text too long
        {
            target[i++]=ch;
            ch = getc(inputfile); //copy name from file to target, one char at a time
        }
    }
    else //entry is not in quotes, so char is currently first letter of string
    {
        while (i<maxchars && ch!=separator && ch!='\n')//stop when you reach separator, end of line, or when text too long
        {
            target[i++]=ch;
            ch = getc(inputfile); //copy name from file to target, one char at a time
        }
    }
    target[i] = '\0';//terminate string
    return target;
}

int readnumberfromfile (int maxvalue,char separator)
{
    int number, i=0;
    char ch;
    char * buff = (char *)malloc(11);//allocate enough space for an 10-digit number and a terminating null
    if (!buff) {printf("Memory allocation failed!\n");return 0;}//return 0 and print error if alloc failed
    if (!maxvalue) maxvalue=MAXINTVALUE;

    ch=getc(inputfile);
    while (!isdigit(ch))
    {
        if (ch == separator||ch=='\n'||ch==EOF) {fprintf(stderr,"Format error in file\n");return 0;}//if no number found(reached separator before digit), print error and return 0
        ch = getc(inputfile);//cycle forward until you reach a digit
    }
    while (i<11 && ch!=separator && ch!='\n')//stop when you reach '~', end of line, or when number too long
    {
        buff[i++]=ch;
        ch = getc(inputfile); //copy number from file to buff, one char at a time
    }
    buff[i] = '\0';//terminate string
    number = atoi(buff)<=maxvalue ? atoi(buff) : maxvalue;//convert string to number and make sure it's in range
    free(buff);
    return number;
}

struct vocab * addtolist(struct vocab * newentry, struct listinfo * list)
{
    if (!list->head)//if head is null, there is no list, so create one
    {
        list->head = list->tail = newentry;//this is the new head and tail
        list->entries = newentry->index = 1;
        newentry->next = NULL;// FISH! not sure if this is necessary, but just be sure...
    }
    else//just appending to the list
    {
        list->tail->next = newentry;//adjust current tail to point to new entry
        list->tail = newentry;//make the new entry the new tail
        newentry->index=++list->entries;
        newentry->next = NULL;
    }
    return newentry;
}

int removefromlist(struct vocab * entry, struct listinfo * list,int freeup)
{
    struct vocab * prev;
    if (list->head == entry) //if entry being deleted is first in the list
    {
        if (list->tail == entry) //if entry is only item in the list
        {
            list->head = list->tail = NULL;
        }
        else //if first in list, but not last
        {
            list->head = entry->next;
        }
    }
    else //entry is not first in list, so set prev to point to previous entry
    {
        prev = list->head;
        while (prev->next!=entry)
        {
            prev=prev->next;
            if (!prev)
            {
                printf("Trying to delete an entry from a list it's not in!!\n");
                return 0;
            }
        }
        if (list->tail == entry)//if entry is at the end of the list
        {
            list->tail = prev;
            list->tail->next = NULL;
        }
        else //if entry is somewhere in middle of list
        {
            prev->next=entry->next;
        }
    }//this entry is now not pointed to in any list
    list->entries--;
    /*following line removed because it could theoretically break a list if the entry was removed from a list after it had been added to another
    entry->next = NULL;//and doesn't point to anything either*/
    reindex(list);
    if (freeup) //if freeup is set, this also wipes the record and frees up the memory associated with it
    {
        free(entry->question);
        free(entry->answer);
        free(entry->info);
        free(entry->hint);
        free(entry);
    }
    return 1;
}

void reindex (struct listinfo * list)
{
    int counter = 1;
    struct vocab * workingentry = list->head;
    while (workingentry)
    {
        workingentry->index = counter++;
        workingentry=workingentry->next;
    }
    if (list->entries!=counter-1) printf("Reindexing Error!\n");
}

int writeliststofile()
{
    int i,counter=0;
    struct listinfo * list;
    struct vocab * entry;
    if (!(outputfile = fopen(DOUTPUTFILENAME, "w")))
    {
        printf ("Error accessing output file!\n");
        return 0;
    }
    else
    {
        printf("Saving...\n");
        for (i=0;i<=3;i++)
        {
            switch (i)
            {
                case 0: list = &n2l;break;
                case 1: list = &norm;break;
                case 2: list = &known;break;
                case 3: list = &old;break;
                default: printf("Loop Error!\n");break;
            }
            entry=list->head;
            while (entry!=NULL)
            {
                if (counter) fprintf(outputfile,"\n");
                fprintf(outputfile,"%s~%s~%s~%s~%i~%i~%i",entry->question,entry->answer,entry->info,entry->hint,entry->right,entry->counter,i);
                entry=entry->next;
                counter++;
            }
        }
        fclose(outputfile);
        printf("...finished. %i entries saved.\n",counter);
        return 1;
    }
}

void testme()
{
    int list_selector, entry_selector, bringupmenu = 0, testagain=1;
    char testmenuchoice = '\n';
    char * youranswer = (char *)malloc(MAXTEXTLENGTH+1);
    struct listinfo * currentlist;
    struct vocab * currententry;
    //debug:printf("%i",__LINE__);
    if (!youranswer) {printf("Memory allocation error!\n");return;}

    while (testagain)
    {
        //debug:fprintf(stderr,"Start of 'testagain' loop\nClearing screen...\n");
        clrscr();

        //select a list at random, using the percentage probabilities in the if statements. FISH! Can this be done with a switch and ranges?
        //debug:fprintf(stderr,"Assigning list selector to random value...");
        list_selector = (((float)rand() / 32768) * 100)+1;
        //debug:fprintf(stderr,"assigned list selector value %i\nAssigning list pointer...",list_selector);
        if (list_selector<33) currentlist = &n2l;
        if (list_selector>32&&list_selector<95) {n2l_flag=0;currentlist=&norm;} //use norm list and cancel n2l flag (not cancelled with other lists)
        if (list_selector>94&&list_selector<100) currentlist = &known;
        if (list_selector==100) currentlist = &old;
        //debug:fprintf(stderr,"assigned list pointer %x\nModifying pointer...",currentlist);

        //do a little control over random selection
        if (currentlist==&n2l && n2l_flag) {currentlist=&norm; n2l_flag=0;} //if n2l list was used last time as well (flag is set), use entry from the norm list instead
        if (currentlist==&n2l) n2l_flag = 1; //is using n2l this time, set flag so it won't be used next time as well

        if (currentlist->entries==0) currentlist = &norm;//if current list is empty, default to normal list
        if (currentlist->entries==0 && !n2l_flag) currentlist = &n2l;//if normal list is empty, try n2l list if it wasn't used last time
        if (currentlist->entries==0 && list_selector%10==5) currentlist = &old;//if list is still empty, in 10% of cases try old list
        if (currentlist->entries==0) currentlist = &known;//in the other 90% of cases, or if old is empty, use the known list
        if (currentlist->entries==0) currentlist = &old;//if known list is empty, try the old list
        if (currentlist->entries==0) {currentlist = &n2l;n2l_flag=1;}//if old list is empty, use n2l list EVEN if it was used last time
        if (currentlist->entries==0) {printf("No entries in list!");return;} //if list is STILL empty, abort
        //debug:fprintf(stderr,"modified list pointer\nAssigning entry selector...");

        //we now have the desired list of words with at least one entry, let's select an entry at random from this list
        entry_selector = (((float)rand() / 32767) * currentlist->entries)+1;
        //debug:fprintf(stderr,"assigned entry selector value %i\nAssignig pointer...",entry_selector);
        currententry = currentlist->head;
        //debug:fprintf(stderr,"set entry pointer to head, going to loop to it...\n");
        while (currententry->index!=entry_selector)
        {
            currententry = currententry->next;//move through list until index matches the random number
            if (currententry==NULL) {printf("Indexing error!\nCurrent list selector: %i, entries: %i, entry selector: %i\n",list_selector,currentlist->entries,entry_selector);return;}//in case not found in list
        }
        //debug:fprintf(stderr,"Looped, testing.\n");

        printf("Translate the following:\n\n\t%s\n\n",currententry->question);
        if (!currententry->info) printf("There is no additional information for this entry.\n");
        else printf("Useful Info: %s\n\n",currententry->info);
        printf("Your Translation:\n\n\t");
        gettextfromkeyboard(youranswer,MAXTEXTLENGTH);
        if (!strcmp(youranswer,currententry->answer))//if you're right
        {
            printf("Yay!\n");

            if(currententry->right) currententry->counter++;
            else currententry->right = currententry->counter = 1;
            if (currententry->counter>2) printf("You answered correctly the last %i times in a row!\n",currententry->counter);
        
            //make comments based on how well it's known, and move to a higher list if appropriate
            if (currentlist==&n2l && currententry->counter>=N2LTONORM)
            {
                removefromlist(currententry,currentlist,0);
                printf("Looks like you know this one a little better now!\nIt will be brought up less frequently.\n");
                currententry->counter = 0;
                addtolist(currententry,&norm);
            }
            if (currentlist==&norm && currententry->counter>=NORMTOKNOWN)
            {
                removefromlist(currententry,currentlist,0);
                printf("Looks like you know this one now!\nIt will be brought up much less frequently.\n");
                currententry->counter = 0;
                addtolist(currententry,&known);
            }
            if (currentlist==&known && currententry->counter>=KNOWNTOOLD)
            {
                removefromlist(currententry,currentlist,0);
                printf("OK! So this one's well-learnt.\nIt probably won't be brought up much any more.\n");
                currententry->counter = 0;
                addtolist(currententry,&old);
            }
        }
    
        else //if you're wrong
        {
            printf("\nSorry, the correct answer is:\n\n\t%s\n\n",currententry->answer);
        
            if(!currententry->right) currententry->counter++;
            else {currententry->right = 0; currententry->counter = 1;}
            if (currententry->counter>1) printf("You've got this one wrong the last %i times.\n",currententry->counter);
            if (currentlist==&norm && currententry->counter>=NORMTON2L)
            {
                removefromlist(currententry,currentlist,0);
                printf("This one could do with some learning...\n");
                currententry->counter = 0;
                addtolist(currententry,&n2l);
            }
            if (currentlist==&known && currententry->counter>=KNOWNTONORM)
            {
                removefromlist(currententry,currentlist,0);
                printf("OK, perhaps you don't know this one as well as you once did...\n");
                currententry->counter = 0;
                addtolist(currententry,&norm);
            }
            if (currentlist==&old && currententry->counter>=OLDTONORM)
            {
                removefromlist(currententry,currentlist,0);
                printf("This old one caught you out, huh? It will be brought up a few more times to help you remember it.\n");
                currententry->counter = 0;
                addtolist(currententry,&norm);
            }
        }

        //debug:fprintf(stderr,"Tested, options menu?\n");
        printf("Type 'o' for options or strike enter for another question\n");
        testmenuchoice = getchar();
        //debug:fprintf(stderr,"Got choice\n");
        if (tolower(testmenuchoice)=='o') bringupmenu = 1;
        //debug:fprintf(stderr,"set menuflag\n");
        if (testmenuchoice!='\n') clearinputbuffer();
        //debug:fprintf(stderr,"cleared getchar\n");
        while (bringupmenu)
        {
            clrscr();
            printf("Current Entry:\n\nQuestion: %s\nAnswer: '%s'\n",currententry->question,currententry->answer);
            if (currententry->info) printf("Info: %s\n",currententry->info); else printf("No info.\n");
            if (currententry->hint) printf("Hint: %s\n\n",currententry->hint); else printf("No hint.\n\n");
            printf("Options Menu:\n\nType q to modify the question phrase displayed for translation.\nType a to change the answer phrase you must provide.\nType i to add/modify additional info for this entry.\nType h to add/modify the hint for this entry.\nType p to mark this entry as high priority to learn.\nType d to delete this entry from the database.\nType x to end testing and return to the main menu.\n\n");
            testmenuchoice=getchar();
            clearinputbuffer();
            switch (testmenuchoice)
            {
                case 'q': printf("Enter new question text for this entry (max %i chars):\n",maxtextlength);
                           currententry->question=gettextfromkeyboard(currententry->question,MAXTEXTLENGTH);
                           break;
                case 'a': printf("Enter new answer text for this entry (max %i chars):\n",maxtextlength);
                           currententry->answer=gettextfromkeyboard(currententry->answer,MAXTEXTLENGTH);
                           break;
                case 'i': printf("Enter new info for this entry (max %i chars):\n",maxtextlength);
                           currententry->info=gettextfromkeyboard(currententry->info,MAXTEXTLENGTH);
                           break;
                case 'h': printf("Enter new hint for this entry (max %i chars):\n",maxtextlength);
                           currententry->hint=gettextfromkeyboard(currententry->hint,MAXTEXTLENGTH);
                           break;
                case 'p': if(currentlist==&n2l)printf("Already marked as priority!\n"); //was using = instead of == in if condition, thank you very much gcc compiler output :-)
                           else
                           {
                               removefromlist(currententry,currentlist,0);
                               currententry->counter = 0;
                               currentlist=&n2l;
                               addtolist(currententry,currentlist);
                               printf("Entry will be brought up more often\n");
                           }
                           break;
                case 'd': printf("Are you sure you want to delete this entry?\nOnce you save, this will be permanent!(y/n)");
                           if (getyesorno()) {removefromlist(currententry,currentlist,1);printf("Entry deleted!\n");bringupmenu=0;}
                           else printf("Entry was NOT deleted.\n");
                           break;
                case 'x': bringupmenu = testagain = 0;
                           break;
                default: printf("Invalid choice.\n");
            }
            if (bringupmenu)
            {
                printf("Select again from the options menu? (y/n)");
                bringupmenu = getyesorno();
            }
            if (!bringupmenu&&testagain)
            {
                printf("Continue testing? (y/n)");
                testagain = getyesorno();
            }
        }
        //debug:fprintf(stderr,"End of 'testagain' loop.\n Clearing Screen...");
//        system("cls");
        clrscr();
//        printf("\f");
    }
    free(youranswer);
//    getchar();
    return;
}

char * gettextfromkeyboard(char * target,int maxchars)
{
    int i =0;
    int memoryallocated_flag =0; //to avoid freeing memory allocated outside function, pointed out by stackoverflow.com/users/688213/mrab
    char ch;//debug:fprintf(stderr,"gettextfromkeyboard started line %i",__LINE__);
    if (!target)//if no memory already allocated (pointer is NULL), do it now
    {
        memoryallocated_flag=1;//debug:printf("%i",__LINE__);
        target=(char *)malloc(maxchars+1);//debug:printf("%i",__LINE__);
        if (!target) {printf("Memory allocation failed!");return NULL;} //debug:printf("%i",__LINE__);//return null if failed
    }
    ch = getchar();
    if (ch=='\n' && memoryallocated_flag) {free(target);return NULL;}//if zero length, free mem (if allocated inside function) and return null pointer
    while (!isalnum(ch))//cycle forward past white space
    {
        ch=getchar();
        if (ch=='\n' && memoryallocated_flag) {free(target);return NULL;}//if all white space, free mem (if allocated inside function) and return null pointer
    }
    while (ch!='\n' && i<maxchars)
    {
        target[i++]=ch;
        ch=getchar();
    }
    target[i]='\0';
    return target;
}

int getyesorno()
{
    char yesorno = '\n';
    while (toupper(yesorno)!='Y'&&toupper(yesorno)!='N')
    {
        yesorno=getchar();
        if (toupper(yesorno)!='Y'&&toupper(yesorno)!='N') printf("Invalid choice. You must enter Y or N:\n");
    }
    clearinputbuffer();
    if (toupper(yesorno)=='Y') return 1;
    else return 0;
}

void wank()
{
    printf("%i",((rand()%13)+1));
    getchar();
}

void testrandom()
{
    
    return;
}

void clrscr()
{
    system(CLEARCOMMAND); //TODO
//    printf("\f");
}

void clearinputbuffer()
{
//    while (getchar()!='\n') getchar();
    char tempchar;
    if (getchar()=='\n') return;
    else while (1)
    {
        tempchar = getchar();
        if (tempchar=='\n') break;
    } //TODO
    return;
}

int main(int argc, char* argv[])
{
    char * inputfilename = DINPUTFILENAME;
//    char * outputfilename = DOUTPUTFILENAME; //TODO not implemented yet
    char separator = '~';
    char menuchoice = '\0';
    n2l.entries = norm.entries = known.entries = old.entries = 0;

    srand((unsigned)time(NULL));

    //debug:fprintf(stderr,"Start...\n");
    printf("Loading...\nLoad default database? (y/n)");
    if (!getyesorno())
    {
        printf("Default file type is .~sv. Import .csv file instead? (y/n)");
        if (getyesorno())
        {
            separator = ',';
            printf("Enter name of .csv file to import:\n");
        }
        else
        {
            printf("Enter name of .~sv file to load:\n");
        }
        inputfilename = gettextfromkeyboard(inputfilename,256);
    }
    getrecordsfromfile(inputfilename,separator);


    while (menuchoice!='x')
    {
        printf("Welcome to the Vocab Test, version C!\n\nMain menu:\n\n\tt: Test Me!\n\ts: Save\n\tx: Exit\n\n");
        menuchoice = getchar();
        if (menuchoice!='\n') clearinputbuffer();
        switch (tolower(menuchoice))
        {
            case 'x': break;
            case 't': testme(); break;
            case 's': writeliststofile();break;
            case 'w': testrandom(); break;
            default: printf("Invalid choice. Please try again.\n"); clearinputbuffer(); break;
        }
        clrscr();
    }


    clrscr();
    printf("Bye for now!\n\nPress enter to exit.");
    getchar();
    //debug:fprintf(stderr,"Successfully closed\n");
    return 0;
}
