#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/time.h>
#include <omp.h>

#define calcIndex(width, x, y)  ((y)*(width) + (x))

void writeVTK2(long timestep, double *data, char prefix[1024], int w, int h) {
    char filename[2048];
    int x, y;

    int offsetX = 0;
    int offsetY = 0;
    float deltax = 1.0;
    long nxy = w * h * sizeof(float);

    snprintf(filename, sizeof(filename), "%s-%05ld%s", prefix, timestep, ".vti");
    FILE *fp = fopen(filename, "w");

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
    fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX,
            offsetX + w, offsetY, offsetY + h, 0, 0, deltax, deltax, 0.0);
    fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
    fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
    fprintf(fp, "</CellData>\n");
    fprintf(fp, "</ImageData>\n");
    fprintf(fp, "<AppendedData encoding=\"raw\">\n");
    fprintf(fp, "_");
    fwrite((unsigned char *) &nxy, sizeof(long), 1, fp);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            float value = data[calcIndex(w, x, y)];
            fwrite((unsigned char *) &value, sizeof(float), 1, fp);
        }
    }

    fprintf(fp, "\n</AppendedData>\n");
    fprintf(fp, "</VTKFile>\n");
    fclose(fp);
}


void show(double *currentfield, int w, int h) {
    printf("\033[H");
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) printf(currentfield[calcIndex(w, x, y)] ? "\033[07m  \033[m" : "  ");
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


void evolve(double *currentfield, double *newfield, int w, int h, int px, int py, int tw, int th) {

#pragma omp parallel num_threads(px*py)
    {

        int this_thread = omp_get_thread_num();

        int x, y;
        int tx = this_thread % px;
        int ty = this_thread / px;
        int offsetX = tx * tw;
        int offsetY = ty * th;
        //printf("THREAD: %i with offset (%i | %i)\n", this_thread, offsetX, offsetY);

        for (y = 0; y < th; y++) {
            for (x = 0; x < tw; x++) {
                int n = countLivingsPeriodic(currentfield, x + offsetX, y + offsetY, w, h);
                int index = calcIndex(w, x + offsetX, y + offsetY);
                if (currentfield[index]) n--;
                newfield[index] = (n == 3 || (n == 2 && currentfield[index]));

                /*
                printf(" INDEX: %d %i(%d|%d) = (%d + %d | %d + %d) \t\t n = %d \talive=%f\n",
                        index,
                        this_thread,
                        x + offsetX,
                        y + offsetY,
                        x,
                       offsetX,
                       y,
                       offsetY,
                       n,
                       currentfield[index]);
                */
            }
        }
    }
}

void filling(double *currentfield, int w, int h) {
    int i;
    for (i = 0; i < h * w; i++) {
        currentfield[i] = (rand() < RAND_MAX / 10) ? 1 : 0; ///< init domain randomly
    }
}

void game(long timeSteps, int tw, int th, int px, int py) {
    int w, h;
    w = tw * px;
    h = th * py;

    double *currentfield = calloc(w * h, sizeof(double));
    double *newfield = calloc(w * h, sizeof(double));

    //printf("size unsigned %d, size long %d\n",sizeof(float), sizeof(long));



    filling(currentfield, w, h);
    long t;
    for (t = 0; t < timeSteps; t++) {
        //show(currentfield, w, h);
        evolve(currentfield, newfield, w, h, px, py, tw, th);

        //printf("%ld timestep\n\n\n\n", t);
        //writeVTK2(t, currentfield, "gol", w, h);

        //usleep(2000);

        //SWAP
        double *temp = currentfield;
        currentfield = newfield;
        newfield = temp;
    }

    free(currentfield);
    free(newfield);

}

int main(int c, char **v) {
    srand(42 * 0x815);
    long n = 0;
    int tw = 0, th = 0, px = 0, py = 0;
    if(c > 1) n = atoi(v[1]);   ///< read timeSteps
    if (c > 2) tw = atoi(v[2]); ///< read thread-width
    if (c > 3) th = atoi(v[3]); ///< read thread-height
    if (c > 4) px = atoi(v[4]); ///< read thread-count X
    if (c > 5) py = atoi(v[5]); ///< read thread-count Y
    if(n <= 0) n = 100;         ///< default timeSteps
    if (tw <= 0) tw = 10;       ///< default thread-width
    if (th <= 0) th = 10;       ///< default thread-height
    if (px <= 0) px = 1;        ///< default thread-count X
    if (py <= 0) py = 1;        ///< default thread-count Y

    game(n, tw, th, px, py);
}
