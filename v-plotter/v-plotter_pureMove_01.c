//Project: V-plotter
//Homepage: www.HomoFaciens.de
//Author Norbert Heinz
//Version: 0.1
//Creation date: 24.06.2015
//This program is free software you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation version 3 of the License.
//For a copy of the GNU General Public License see http://www.gnu.org/licenses/
//
//compile with gcc v-plotter.c -o v-plotter -I/usr/local/include -L/usr/local/lib -lwiringPi -lm
//For details see:
//http://www.HomoFaciens.de/technics-machines-v-plotter_en_navion.htm
//fsp: latest update 20161031

#include <stdio.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>
#include <wiringPi.h>
#include <unistd.h>


#define PI 3.1415927
//---#define SERVOUP        10
//---#define SERVODOWN      20
//fsp: change from 10 (0 degrees)/20 (90 degrees) to 20/40
//SERVOUP = lift Pin
#define SERVOUP        11
//SERVODOWN = Pin down = servo points downwards in my case
#define SERVODOWN      5
//fsp GPIO Pin numbers:
#define LEFT_STEPPER01 11
#define LEFT_STEPPER02 10

#define LEFT_STEPPER03  6
#define LEFT_STEPPER04 14


#define RIGHT_STEPPER01  2
#define RIGHT_STEPPER02  3

#define RIGHT_STEPPER03 12
#define RIGHT_STEPPER04 13

#define STEP_PAUSE 1500

#define STEP_MAX 220.0

#define Z_SERVO 0

#define BUFFERSIZE         120

//Lengths given in millimiters
//#define BASELENGTH              845
//#define CORDLENGTH_LEFT         220
//#define CORDLENGTH_RIGHT        667
//fsp entries: 20160427
//URL: http://www.homofaciens.de/technics-machines-v-plotter_ge_navion.htm
//Die Software benötigt vier Parameter:
//Die Basislänge, also den Abstand der beiden Schrittmotoren, die Länge beider Schnüre im Nullpunkt (0,0)
//sowie die Schnurlänge pro Schritt eines Motors. 
//Dazu wird einer der Motoren um 2000 Schritte bewegt und die Längendifferenz der Schnur gemessen. -> StepsPermm
//Diese Parameter sind im Quellcode zu setzen. 
//#define BASELENGTH              700
//#define CORDLENGTH_LEFT         210
//#define CORDLENGTH_RIGHT        550
#define BASELENGTH              2310
#define CORDLENGTH_LEFT         1190
#define CORDLENGTH_RIGHT        1560

//Correction for errors caused by flexibility of the cord 0.0 = OFF (experimental)
#define CORDFLEXFACTOR    0.0f

int  MaxRows = 24;
int  MaxCols = 80;
int  MessageX = 1;
int  MessageY = 24;
unsigned char MoveBuffer[BUFFERSIZE];
long currentX = 0, currentY = 0;
long CordLengthLeft = 0;
long CordLengthRight = 0;
long BaseLength = 0;
long X0, Y0;
int  FilesFound = 0;
int currentPlotDown = 0;
int BoldLineWidth = 0;
int BoldLineGap = 0;
long BoldLineX = 0, BoldLineY = 0;
//fsp added 20160514:
int penIsDown = 0;

int StepX = 0; 
int StepY = 0;

//Steps per mm (with gear)
//double StepsPermm = 2000.0 / 98.0;

//Steps per mm (without gear)
//double StepsPermm = 2000.0 / 199.0;
//fsp: 19.5cm per 2000 steps:
//double StepsPermm = 2000.0 / 765.0;
//update done for smaller wheels 13.5cm per 1000 steps:
double StepsPermm = 1000.0 / 135.0;
//double StepsPermm = 2000.0 / 1265.0;

char PicturePath[1000];

//+++++++++++++++++++++++ Start gotoxy ++++++++++++++++++++++++++
//Thanks to 'Stack Overflow', found on http://www.daniweb.com/software-development/c/code/216326
int gotoxy(int x, int y) {
  char essq[100]; // String variable to hold the escape sequence
  char xstr[100]; // Strings to hold the x and y coordinates
  char ystr[100]; // Escape sequences must be built with characters
   
  //Convert the screen coordinates to strings.
  sprintf(xstr, "%d", x);
  sprintf(ystr, "%d", y);
   
  //Build the escape sequence (vertical move).
  essq[0] = '\0';
  strcat(essq, "\033[");
  strcat(essq, ystr);
   
  //Described in man terminfo as vpa=\E[%p1%dd. Vertical position absolute.
  strcat(essq, "d");
   
  //Horizontal move. Horizontal position absolute
  strcat(essq, "\033[");
  strcat(essq, xstr);
  // Described in man terminfo as hpa=\E[%p1%dG
  strcat(essq, "G");
   
  //Execute the escape sequence. This will move the cursor to x, y
  printf("%s", essq);
  return 0;
}
//------------------------ End gotoxy ----------------------------------

//+++++++++++++++++++++++ Start clrscr ++++++++++++++++++++++++++
void clrscr(int StartRow, int EndRow) {
  int i, i2;
  
  if (EndRow < StartRow){
    i = EndRow;
    EndRow = StartRow;
    StartRow = i;
  }
  gotoxy(1, StartRow);
  for (i = 0; i <= EndRow - StartRow; i++){
    for(i2 = 0; i2 < MaxCols; i2++){
      printf(" ");
    }
    printf("\n");
  }
}
//----------------------- End clrscr ----------------------------

//+++++++++++++++++++++++ Start kbhit ++++++++++++++++++++++++++++++++++
//Thanks to Undertech Blog, http://www.undertec.de/blog/2009/05/kbhit_und_getch_fur_linux.html
int kbhit(void) {

   struct termios term, oterm;
   int fd = 0;
   int c = 0;
   
   tcgetattr(fd, &oterm);
   memcpy(&term, &oterm, sizeof(term));
   term.c_lflag = term.c_lflag & (!ICANON);
   term.c_cc[VMIN] = 0;
   term.c_cc[VTIME] = 1;
   tcsetattr(fd, TCSANOW, &term);
   c = getchar();
   tcsetattr(fd, TCSANOW, &oterm);
   if (c != -1)
   ungetc(c, stdin);

   return ((c != -1) ? 1 : 0);

}
//------------------------ End kbhit -----------------------------------

//+++++++++++++++++++++++ Start getch ++++++++++++++++++++++++++++++++++
//Thanks to Undertech Blog, http://www.undertec.de/blog/2009/05/kbhit_und_getch_fur_linux.html
int getch(){
   static int ch = -1, fd = 0;
   struct termios new, old;

   fd = fileno(stdin);
   tcgetattr(fd, &old);
   new = old;
   new.c_lflag &= ~(ICANON|ECHO);
   tcsetattr(fd, TCSANOW, &new);
   ch = getchar();
   tcsetattr(fd, TCSANOW, &old);

//   printf("ch=%d ", ch);

   return ch;
}
//------------------------ End getch -----------------------------------

//++++++++++++++++++++++ Start MessageText +++++++++++++++++++++++++++++
void MessageText(char *message, int x, int y, int alignment){
  int i;
  char TextLine[300];

  clrscr(y, y);
  gotoxy (x, y);
  
  TextLine[0] = '\0';
  if(alignment == 1){
    for(i=0; i < (MaxCols - strlen(message)) / 2 ; i++){
      strcat(TextLine, " ");
    }
  }
  strcat(TextLine, message);
  
  printf("%s\n", TextLine);
}
//-------------------------- End MessageText ---------------------------

//++++++++++++++++++++++ Start PrintRow ++++++++++++++++++++++++++++++++
void PrintRow(char character, int y){
  int i;
  gotoxy (1, y);
  for(i=0; i<MaxCols;i++){
    printf("%c", character);
  }
}
//-------------------------- End PrintRow ------------------------------

//+++++++++++++++++++++++++ ErrorText +++++++++++++++++++++++++++++
void ErrorText(char *message){
  clrscr(MessageY + 2, MessageY + 2);
  gotoxy (1, MessageY + 2);  
  printf("Last error: %s", message);
}
//----------------------------- ErrorText ---------------------------

//+++++++++++++++++++++++++ PrintMenue_01 ++++++++++++++++++++++++++++++
void PrintMenue_01(char * PlotFile, double scale, double width, double height, long MoveLength){
  char TextLine[300];
  int baselength=BASELENGTH;
  
   clrscr(1, MessageY-2);
   MessageText("/// Digitale Düsseldorf Vertikalplotter ///", 1, 1, 1);
   sprintf(TextLine, "m            - toggle move length, current value = %ld step(s)", MoveLength);
   MessageText(TextLine, 10, 3, 0);
   MessageText("Cursor right - move plotter in positive X direction", 10, 4, 0);
   MessageText("Cursor left  - move plotter in negative X direction", 10, 5, 0);
   MessageText("Cursor up    - move plotter in positive Y direction", 10, 6, 0);
   MessageText("Cursor down  - move plotter in negative Y direction", 10, 7, 0);
   //MessageText("Page up      - lift pen", 10, 8, 0);
   //MessageText("Page down    - touch down pen", 10, 9, 0);
   MessageText("u = up       - lift pen", 10, 8, 0);
   MessageText("d = down     - touch down pen", 10, 9, 0);
   sprintf(TextLine, "f            - choose file. Current file = \"%s\"", PlotFile);
   MessageText(TextLine, 10, 10, 0);
   MessageText("0            - move plotter to 0/0", 10, 11, 0);
   sprintf(TextLine, "s            - Scale set to = %0.4f. W = %0.2fcm, H = %0.2fcm", scale, width * scale / 1000.0, height * scale / 1000.0);
   MessageText(TextLine, 10, 12, 0);
   sprintf(TextLine, "info         - BASELENGTH = %d, CORDLENGTH_L = %d, CORDLENGTH_R = %d", baselength, CORDLENGTH_LEFT, CORDLENGTH_RIGHT);
   MessageText(TextLine, 10, 14, 0);
   sprintf(TextLine, "b            - Bold line = %d steps", BoldLineWidth);
   MessageText(TextLine, 10, 15, 0);
   MessageText("p            - plot file", 10, 16, 0);

   MessageText("Esc          - leave program", 10, 17, 0);
   
}
//------------------------- PrintMenue_01 ------------------------------

//+++++++++++++++++++++++++ PrintMenue_02 ++++++++++++++++++++++++++++++
char *PrintMenue_02(int StartRow, int selected){
  char TextLine[300];
  char FilePattern[5];
  char OpenDirName[1000];
  static char FileName[101];
  DIR *pDIR;
  struct dirent *pDirEnt;
  int i = 0;  
  int Discard = 0;
  
  clrscr(1, MessageY-2);
  MessageText("*** Choose plotter file ***", 1, 1, 1);
   
  strcpy(OpenDirName, PicturePath);
  

  pDIR = opendir(OpenDirName);
  if ( pDIR == NULL ) {
    sprintf(TextLine, "Could not open directory '%s'!", OpenDirName);
    MessageText(TextLine, 1, 4, 1);
    getch();
    return( "" );
  }
  
  FilesFound = 0;
  pDirEnt = readdir( pDIR );
  while ( pDirEnt != NULL && i < 20) {
    if(strlen(pDirEnt->d_name) > 4){
      if(memcmp(pDirEnt->d_name + strlen(pDirEnt->d_name)-4, ".svg",4) == 0){
        FilesFound++;
        if(Discard >= StartRow){
          if(i + StartRow == selected){
            sprintf(TextLine, ">%s<", pDirEnt->d_name);
            strcpy(FileName, pDirEnt->d_name);
          }
          else{
            sprintf(TextLine, " %s ", pDirEnt->d_name); 
          }
          MessageText(TextLine, 1, 3 + i, 0);
          i++;
        }
        Discard++;

      }
    }
    pDirEnt = readdir( pDIR );
  }  

  gotoxy(MessageX, MessageY + 1);
  printf("Choose file using up/down keys and confirm with 'Enter' or press 'Esc' to cancel.");
  

  return (FileName);
}
//------------------------- PrintMenue_02 ------------------------------


//+++++++++++++++++++++++++ PrintMenue_03 ++++++++++++++++++++++++++++++
void PrintMenue_03(char *FullFileName, long NumberOfLines, long CurrentLine, long CurrentX, long CurrentY, long StartTime){
  char TextLine[300];
  long CurrentTime, ProcessHours = 0, ProcessMinutes = 0, ProcessSeconds = 0;
  
   CurrentTime = time(0);
   
   CurrentTime -= StartTime;
   
   while (CurrentTime > 3600){
     ProcessHours++;
     CurrentTime -= 3600;
   }
   while (CurrentTime > 60){
     ProcessMinutes++;
     CurrentTime -= 60;
   }
   ProcessSeconds = CurrentTime;
   
   clrscr(1, MessageY - 2);
   MessageText("*** Plotting file ***", 1, 1, 1);
   
   sprintf(TextLine, "File name: %s", FullFileName);
   MessageText(TextLine, 10, 3, 0);
   sprintf(TextLine, "Number of lines: %ld", NumberOfLines);
   MessageText(TextLine, 10, 4, 0);
   sprintf(TextLine, "Current Position(%ld): X = %ld, Y = %ld     ", CurrentLine, CurrentX, CurrentY);
   MessageText(TextLine, 10, 5, 0);
   sprintf(TextLine, "Process time: %02ld:%02ld:%02ld", ProcessHours, ProcessMinutes, ProcessSeconds);
   MessageText(TextLine, 10, 6, 0);
     

}
//------------------------- PrintMenue_03 ------------------------------



//++++++++++++++++++++++++++++++ MakeStepLeft ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void MakeStepLeft(int direction){
  StepX += direction;
  
  if(StepX > 3){
    StepX = 0;
  }
  if(StepX < 0){
    StepX = 3;
  }  
  
  if(StepX == 0){
    digitalWrite(LEFT_STEPPER01, 1);
    usleep(STEP_PAUSE);
    digitalWrite(LEFT_STEPPER02, 0);
    digitalWrite(LEFT_STEPPER03, 0);
    digitalWrite(LEFT_STEPPER04, 0);    
  }
  if(StepX == 1){
    digitalWrite(LEFT_STEPPER03, 1);
    usleep(STEP_PAUSE);
    digitalWrite(LEFT_STEPPER01, 0);
    digitalWrite(LEFT_STEPPER02, 0);
    digitalWrite(LEFT_STEPPER04, 0);    
  }
  if(StepX == 2){
    digitalWrite(LEFT_STEPPER02, 1);
    usleep(STEP_PAUSE);
    digitalWrite(LEFT_STEPPER01, 0);
    digitalWrite(LEFT_STEPPER03, 0);
    digitalWrite(LEFT_STEPPER04, 0);    
  }
  if(StepX == 3){
    digitalWrite(LEFT_STEPPER04, 1);    
    usleep(STEP_PAUSE);
    digitalWrite(LEFT_STEPPER01, 0);
    digitalWrite(LEFT_STEPPER02, 0);
    digitalWrite(LEFT_STEPPER03, 0);
  }
  
  usleep(STEP_PAUSE);
}

//++++++++++++++++++++++++++++++ MakeStepRight ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void MakeStepRight(int direction){
  StepY += direction;
  
  if(StepY > 3){
    StepY = 0;
  }
  if(StepY < 0){
    StepY = 3;
  }  
  

  if(StepY == 0){
    digitalWrite(RIGHT_STEPPER01, 1);
    usleep(STEP_PAUSE);
    digitalWrite(RIGHT_STEPPER02, 0);
    digitalWrite(RIGHT_STEPPER03, 0);
    digitalWrite(RIGHT_STEPPER04, 0);    
  }
  if(StepY == 1){
    digitalWrite(RIGHT_STEPPER03, 1);
    usleep(STEP_PAUSE);
    digitalWrite(RIGHT_STEPPER01, 0);
    digitalWrite(RIGHT_STEPPER02, 0);
    digitalWrite(RIGHT_STEPPER04, 0);    
  }
  if(StepY == 2){
    digitalWrite(RIGHT_STEPPER02, 1);
    usleep(STEP_PAUSE);
    digitalWrite(RIGHT_STEPPER01, 0);
    digitalWrite(RIGHT_STEPPER03, 0);
    digitalWrite(RIGHT_STEPPER04, 0);    
  }
  if(StepY == 3){
    digitalWrite(RIGHT_STEPPER04, 1);    
    usleep(STEP_PAUSE);
    digitalWrite(RIGHT_STEPPER01, 0);
    digitalWrite(RIGHT_STEPPER02, 0);
    digitalWrite(RIGHT_STEPPER03, 0);
  }

//  printf("StepY\n");
  usleep(STEP_PAUSE);
}


//++++++++++++++++++++++++++++++++++++++ moveXY +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void moveXY(long X, long Y){
  long newCordLengthLeft;
  long newCordLengthRight;
  double forceLeft, forceRight;
  double deltaCordLeft, deltaCordRight;
  double deltaCordLeft0, deltaCordRight0;
  double alpha;
  char TextLine[1000];
  int waitTime = 6001;
  int waitTimePenDown = waitTime;
  int waitTimePenUp = 10;

  //fsp: make moving speed dependent on pen up/down status:
  if (currentPlotDown == 1){
    waitTime = waitTimePenDown;
  } else {
    waitTime = waitTimePenUp;
  }

  //calculate initial stretch of cords (experimental)
  alpha = atan((double)(X0) / (double)(Y0));
  forceLeft = 1.0 * cos(alpha);
  forceRight = 1.0 * sin(alpha);

  deltaCordLeft0 = (double)(CORDLENGTH_LEFT * StepsPermm) * forceLeft * CORDFLEXFACTOR;
  deltaCordRight0 = (double)(CORDLENGTH_RIGHT * StepsPermm) * forceRight * CORDFLEXFACTOR;
  

  //forces at current coordinates (experimental)
  alpha = atan((double)(X + X0) / (double)(Y + Y0));
  forceLeft = 1.0 * cos(alpha);
  forceRight = 1.0 * sin(alpha);
  
  X += X0;
  Y += Y0;
    
  newCordLengthLeft = sqrt((double)(X * X) + (double)(Y * Y));
  newCordLengthRight = sqrt((double)((BaseLength-X) * (BaseLength-X)) + (double)(Y * Y));
  
  deltaCordLeft = (double)(newCordLengthLeft) * forceLeft * CORDFLEXFACTOR - deltaCordLeft0;
  deltaCordRight = (double)(newCordLengthRight) * forceRight * CORDFLEXFACTOR - deltaCordRight0;
  

  newCordLengthLeft -= deltaCordLeft;
  newCordLengthRight -= deltaCordRight;
  
  while(newCordLengthLeft > CordLengthLeft){
    MakeStepLeft(1);
	usleep(waitTime);
    CordLengthLeft++;
  }
  while(newCordLengthLeft < CordLengthLeft){
    MakeStepLeft(-1);
	usleep(waitTime);
    CordLengthLeft--;
  }

  while(newCordLengthRight > CordLengthRight){
    MakeStepRight(1);
	usleep(waitTime);
    CordLengthRight++;
  }
  while(newCordLengthRight < CordLengthRight){
    MakeStepRight(-1);
	usleep(waitTime);
    CordLengthRight--;
  }


}

//++++++++++++++++++++++++++++++++++++++ BoldLinePattern ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void BoldLinePattern(long X, long Y){
  int i;
  long xDiff, yDiff;
  double alpha = 0.0;
  long xCircle, yCircle;
  
  xDiff = BoldLineX - X;
  yDiff = BoldLineY - Y;
  
  if(BoldLineWidth > 0){
    if(currentPlotDown == 1){    
      if(sqrt(xDiff * xDiff + yDiff * yDiff) > BoldLineGap){
        BoldLineX = X;
        BoldLineY = Y;
        
        for(alpha = 0.0; alpha < 2.0 * PI; alpha += PI / 2500.0){
          xCircle = cos(alpha) * BoldLineWidth + X;
          yCircle = sin(alpha) * BoldLineWidth + Y;
          moveXY(xCircle, yCircle);
        }
        moveXY(X, Y);
      }
    }//if(currentPlotDown == 1){
  }
}

//++++++++++++++++++++++++++++++++++++++ CalculateLine ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
int CalculateLine(long moveToX, long moveToY){
  char TextLine[1000] = "";
  long  tempX = 0, tempY = 0;
  int i = 0;
  
  sprintf(TextLine, "Moving X: %ld, Moving Y: %ld", moveToX, moveToY);
  MessageText(TextLine, MessageX, MessageY, 0);
//  getch();


  if(moveToX - currentX != 0 && moveToY - currentY != 0){
    tempX = currentX;
    tempY = currentY;
    if(abs(moveToX - currentX) > abs(moveToY - currentY)){
      while(currentX < moveToX){
        currentX++;
        moveXY(currentX, currentY);
        currentY = tempY + (currentX - tempX) * (moveToY - tempY) / (moveToX - tempX);
        moveXY(currentX, currentY);
        BoldLinePattern(currentX, currentY);
      }
      while(currentX > moveToX){
        currentX--;
        moveXY(currentX, currentY);
        currentY = tempY + (currentX - tempX) * (moveToY - tempY) / (moveToX - tempX);
        moveXY(currentX, currentY);
        BoldLinePattern(currentX, currentY);
      }
    }
    else{
      while(currentY < moveToY){
        currentY++;
        moveXY(currentX, currentY);
        currentX = tempX + (currentY - tempY) * (moveToX - tempX) / (moveToY - tempY);
        moveXY(currentX, currentY);
        BoldLinePattern(currentX, currentY);
      }
      while(currentY > moveToY){
        currentY--;
        moveXY(currentX, currentY);
        currentX = tempX + (currentY - tempY) * (moveToX - tempX) / (moveToY - tempY);
        moveXY(currentX, currentY);
        BoldLinePattern(currentX, currentY);
      }
    }    
  }


  while(moveToY > currentY){
    currentY++;
    moveXY(currentX, currentY);
    BoldLinePattern(currentX, currentY);
  }
  while(moveToY < currentY){
    currentY--;
    moveXY(currentX, currentY);
    BoldLinePattern(currentX, currentY);
  }

  while(moveToX > currentX){
    currentX++;
    moveXY(currentX, currentY);
    BoldLinePattern(currentX, currentY);
  }
  while(moveToX < currentX){
    currentX--;
    moveXY(currentX, currentY);
    BoldLinePattern(currentX, currentY);
  }

  return 0; 
}
//-------------------------------------- CalculateLine --------------------------------------------------------

//######################################################################
//################## Main ##############################################
//######################################################################

int main(int argc, char **argv){

  int MenueLevel = 0;
  int KeyHit = 0;
  int KeyCode[5];
  char FileInfo[3];
  char FileName[200] = "";
  char FullFileName[200] = "";
  char FileNameOld[200] = "";
  struct winsize terminal;
  double Scale = 1.0;
  double OldScale = 1.0;
  long MoveLength = 1;
  int i;
  int SingleKey=0;
  long currentPlotX = 0, currentPlotY = 0;
  int FileSelected = 0;
  int FileStartRow = 0;
  char *pEnd;
  FILE *PlotFile;
  char TextLine[10000];
  long xMin = 1000000, xMax = -1000000;
  long yMin = 1000000, yMax = -1000000;
  long coordinateCount = 0;
  char a;
  int ReadState = 0;
  long xNow = 0, yNow = 0;
  long xNow1 = 0, yNow1 = 0;
  long xNow2 = 0, yNow2 = 0;
  struct timeval StartTime, EndTime;
  long coordinatePlot = 0;
  int stopPlot = 0;
  long PlotStartTime = 0;
  int MaxFileRows = 0;

  if (wiringPiSetup () == -1){
    printf("Could not run wiringPiSetup!");
    exit(1);
  }

  softPwmCreate(Z_SERVO, SERVOUP, 200);
  softPwmWrite(Z_SERVO, SERVOUP);
  //fsp: value in microseconds, i.e. 500.000 microsecs = 0.5 secs.
  usleep(500000);
  softPwmWrite(Z_SERVO, 0);


  pinMode (LEFT_STEPPER01, OUTPUT);
  pinMode (LEFT_STEPPER02, OUTPUT);
  pinMode (LEFT_STEPPER03, OUTPUT);
  pinMode (LEFT_STEPPER04, OUTPUT);

  digitalWrite(LEFT_STEPPER01, 1);
  digitalWrite(LEFT_STEPPER02, 0);    
  digitalWrite(LEFT_STEPPER03, 0);    
  digitalWrite(LEFT_STEPPER04, 0);    

  pinMode (RIGHT_STEPPER01, OUTPUT);
  pinMode (RIGHT_STEPPER02, OUTPUT);
  pinMode (RIGHT_STEPPER03, OUTPUT);
  pinMode (RIGHT_STEPPER04, OUTPUT);

  digitalWrite(RIGHT_STEPPER01, 1);
  digitalWrite(RIGHT_STEPPER02, 0);    
  digitalWrite(RIGHT_STEPPER03, 0);    
  digitalWrite(RIGHT_STEPPER04, 0);    

        for(i=0; i<MoveLength; i++){
          MakeStepLeft(-1);
        }
        for(i=0; i<1000; i++){
          MakeStepRight(-1);
        }
        for(i=0; i<1000; i++){
          MakeStepRight(1);
        }

//fsp: on exit, switch all motors off:
  digitalWrite(LEFT_STEPPER01, 0);
  digitalWrite(LEFT_STEPPER02, 0);
  digitalWrite(LEFT_STEPPER03, 0);
  digitalWrite(LEFT_STEPPER04, 0);

  digitalWrite(RIGHT_STEPPER01, 0);
  digitalWrite(RIGHT_STEPPER02, 0);
  digitalWrite(RIGHT_STEPPER03, 0);
  digitalWrite(RIGHT_STEPPER04, 0);
  //exit(0);

  return 0;
}
