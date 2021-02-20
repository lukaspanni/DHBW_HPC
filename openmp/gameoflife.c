#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>
#include <omp.h>

#define calcIndex(width, x,y)  ((y)*(width) + (x))

long TimeSteps = 5;

void writeVTK2(long timestep, double *data, char prefix[1024], int w, int h) {
  char filename[2048];  
  int x,y; 
  
  int offsetX=0;
  int offsetY=0;
  float deltax=1.0;
  long  nxy = w * h * sizeof(float);  

  snprintf(filename, sizeof(filename), "%s-%05ld%s", prefix, timestep, ".vti");
  FILE* fp = fopen(filename, "w");

  fprintf(fp, "<?xml version=\"1.0\"?>\n");
  fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
  fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX, offsetX + w, offsetY, offsetY + h, 0, 0, deltax, deltax, 0.0);
  fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
  fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
  fprintf(fp, "</CellData>\n");
  fprintf(fp, "</ImageData>\n");
  fprintf(fp, "<AppendedData encoding=\"raw\">\n");
  fprintf(fp, "_");
  fwrite((unsigned char*)&nxy, sizeof(long), 1, fp);

  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      float value = data[calcIndex(h, x,y)];
      fwrite((unsigned char*)&value, sizeof(float), 1, fp);
    }
  }
  
  fprintf(fp, "\n</AppendedData>\n");
  fprintf(fp, "</VTKFile>\n");
  fclose(fp);
}


void show(double* currentfield, int w, int h) {
  printf("\033[H");
  int x,y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) printf(currentfield[calcIndex(w, x,y)] ? "\033[07m  \033[m" : "  ");
    printf("\033[E");
    printf("\n");
  }
  fflush(stdout);
}
 
int countLivingsPeriodic(double *currentfield, int x, int y, int w, int h) {
    int n = 0;
    for (int y1 = y - 1; y1 <= y + 1; y1++) {
        for (int x1 = x - 1; x1 <= x + 1; x1++) {
            if (currentfield[calcIndex(w, (x1 + w) % w, (y1 + h) % h)]) {
                n++;
            }
        }
    }
    return n;
}  


void evolve(double* currentfield, double* newfield, int w, int h, int px, int py, int tw, int th) {
  #pragma omp parallel num_threads(px*py)
  {
    int x, y;
    int this_thread = omp_get_thread_num();
    //printf("\n\nTHREAD: %ld\n\n", this_thread);
    int tx = this_thread % tw;
    int ty = this_thread / tw;
    int offsetX = tx * tw;
    int offsetY = ty * th;

    for (y = 0; y < th; y++) {
      for (x = 0; x < tw; x++) {
        int n = countLivingsPeriodic(currentfield, x + offsetX, y + offsetY, w, h);
        int index = calcIndex(w, x + offsetX, y + offsetY);
        //printf(" INDEX: %d %ld(%d|%d)\n", index, this_thread, x + offsetX, y + offsetY);
              if (currentfield[index]) n--;
              newfield[index] = (n == 3 || (n == 2 && currentfield[index]));
      }
    }
  }
}
 
void filling(double* currentfield, int w, int h) {
  int i;
  for (i = 0; i < h*w; i++) {
    currentfield[i] = (rand() < RAND_MAX / 10) ? 1 : 0; ///< init domain randomly
  }
}
 
void game(int tw, int th) {
  int px, py;
  px = 2;
  py = 2;

  int w, h;
  w = tw * px;
  h = th * py;

  double *currentfield = calloc(w*h, sizeof(double));
  double *newfield     = calloc(w*h, sizeof(double));
  
  //printf("size unsigned %d, size long %d\n",sizeof(float), sizeof(long));
  
 


  filling(currentfield, w, h);
  long t;
  for (t=0;t<TimeSteps;t++) {
    //show(currentfield, w, h);
    evolve(currentfield, newfield, w, h, px, py, tw, th);
    
    printf("%ld timestep\n",t);
    writeVTK2(t,currentfield,"gol", w, h);
    
    usleep(2000);

    //SWAP
    double *temp = currentfield;
    currentfield = newfield;
    newfield = temp;
  }
  
  free(currentfield);
  free(newfield);
  
}
 
int main(int c, char **v) {
  srand(42);
  int tw = 0, th = 0;
  if (c > 1) tw = atoi(v[1]); ///< read width
  if (c > 2) th = atoi(v[2]); ///< read height
  if (tw <= 0) tw = 4; ///< default thread-width
  if (th <= 0) th = 4; ///< default thread-height
  game(tw, th);
}
