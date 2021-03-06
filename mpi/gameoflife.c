#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mpi/mpi.h>

#define calcIndex(width, x, y)  ((y)*(width) + (x))

//#define performance

void writeVTK2(long timestep, const double *data, char prefix[1024], int w, int tw, int th, int offsetX, int offsetY) {
    char filename[2048];
    int x, y;

    // int offsetX = 0;
    // int offsetY = 0;
    float deltax = 1.0;
    long nxy = tw * th * sizeof(float);

    int threadnum = omp_get_thread_num();

    snprintf(filename, sizeof(filename), "%s-%05ld-%03d%s", prefix, timestep, threadnum, ".vti");
    FILE *fp = fopen(filename, "w");

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
    fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX,
            offsetX + tw, offsetY, offsetY + th, 0, 0, deltax, deltax, 0.0);
    fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
    fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
    fprintf(fp, "</CellData>\n");
    fprintf(fp, "</ImageData>\n");
    fprintf(fp, "<AppendedData encoding=\"raw\">\n");
    fprintf(fp, "_");
    fwrite((unsigned char *) &nxy, sizeof(long), 1, fp);

    for (y = 0; y < th; y++) {
        for (x = 0; x < tw; x++) {
            float value = data[calcIndex(w, x + offsetX, y + offsetY)];
            fwrite((unsigned char *) &value, sizeof(float), 1, fp);
        }
    }

    fprintf(fp, "\n</AppendedData>\n");
    fprintf(fp, "</VTKFile>\n");
    fclose(fp);
}

void writeVTK2_parallel(long timestep, char prefix[1024], char vti_prefix[1024], int w, int h, int px, int py){
    char filename[2048];

    snprintf(filename, sizeof(filename), "%s-%05ld%s", prefix, timestep, ".pvti");
    FILE *fp = fopen(filename, "w");

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, "<VTKFile type=\"PImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");

    fprintf(fp, "<PImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", 0,
                w, 0, h, 0, 0, 1.0, 1.0, 0.0);
    fprintf(fp, "<PCellData Scalars=\"%s\">\n", vti_prefix);
    fprintf(fp, "<PDataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", vti_prefix);
    fprintf(fp, "</PCellData>\n");

    for(int x = 0; x < px; x++){
        for(int y = 0; y < py; y++){
            
            int start_x = x * (w/px);
            int end_x = start_x + (w/px);
            int start_y = y * (h/py);
            int end_y = start_y + (w/py);

            char file[2048];
            snprintf(file, sizeof(file), "%s-%05ld-%03d%s", vti_prefix, timestep, px * y + x, ".vti");

            fprintf(fp, "<Piece Extent=\"%d %d %d %d 0 0\" Source=\"%s\"/>\n", start_x, end_x, start_y, end_y, file);
        }
    }
    fprintf(fp, "</PImageData>\n");

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


void evolve(int timestep, double *currentfield, double *newfield, int w, int h, int px, int py, int tw, int th) {

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
#ifndef performance
        writeVTK2(timestep, currentfield, "gol", w, tw, th, offsetX, offsetY);
#endif
    }
}

void fillRandom(double *currentField, int w, int h) {
    for (int i = 0; i < h * w; i++) {
        currentField[i] = (rand() < RAND_MAX / 10) ? 1 : 0; ///< init domain randomly
    }
}

void readUntilNewLine(FILE *pFile) {
    int readCharacter;
    do {
        readCharacter = fgetc(pFile);
    } while (readCharacter != '\n' && readCharacter != EOF);
}

void setCellState(double *currentField, int alive, int *index, int *count) {
    //printf("Filling %d characters at index %d with %d\n", (*count), (*index), alive);
    while((*count) > 0) {
        currentField[(*index)] = (double)alive;
        //printf("Setting %d to %d\n", (*index), alive);
        (*index)++;
        (*count)--;
    }
}

void fillFromFile(double *currentField, int w, char *fileName) {
    FILE *file = fopen(fileName, "r");

    //Files start with a 2 line header (comment + size/rule info)
    readUntilNewLine(file);
    readUntilNewLine(file);

    int index = 0;
    int count = 0;


    int readCharacter = fgetc(file);
    while(readCharacter != EOF) {
        if(readCharacter >= '0' && readCharacter <= '9') {
            count = count * 10 + (readCharacter - '0');
        } else if(readCharacter == 'b' || readCharacter == 'o') {
            int alive = readCharacter == 'o';
            if(count == 0) {
                //Direct color switch without preceding number
                count = 1;
            }
            setCellState(currentField, alive, &index, &count);
        } else if(readCharacter == '$' || readCharacter == '!') {
            //End of line, or end of file -> Pad until end of line with empty cells
            if((index % w) != 0) {
                count = (w - (index % w));
            }
            setCellState(currentField, 0, &index, &count);
        } else if(readCharacter == '#') {
            readUntilNewLine(file);
        } else if(readCharacter == '\n') {

        }
        else {
            printf("Invalid character read: %d", readCharacter);
            exit(1);
        }

        readCharacter = fgetc(file);
    }
}

void filling(double *currentField, int w, int h, char *fileName) {
    if (access(fileName, R_OK) == 0) {
        //File exists
        fillFromFile(currentField, w, fileName);
    } else {
        //File does not exist
        fillRandom(currentField, w, h);
    }
}

void game(long timeSteps, int tw, int th, int px, int py) {
    int w, h;
    w = tw * px;
    h = th * py;

    double *currentfield = calloc(w * h, sizeof(double));
    double *newfield = calloc(w * h, sizeof(double));

    //printf("size unsigned %d, size long %d\n",sizeof(float), sizeof(long));


    filling(currentfield, w, h, "file.rle");
    long t;
    for (t = 0; t < timeSteps; t++) {
        //show(currentfield, w, h);
        //evolve(t, currentfield, newfield, w, h, px, py, tw, th);

#ifndef performance
        writeVTK2_parallel(t, "golp", "gol", w, h, px, py);    

        printf("%ld timestep\n", t);
#endif
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

    int commSize, rank;

    MPI_Init(&c, &v);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    printf("Initialized, Size: %d, Rank: %d\n", commSize, rank);

    MPI_Comm comm;
    int dimensions[] = {commSize};
    int periodic[] = {1};
    MPI_Cart_create(MPI_COMM_WORLD, 1, dimensions, periodic, 0, &comm);

    int coordinates[1];
    MPI_Cart_coords(comm, rank, 1, &coordinates);

    int right, left;
    MPI_Cart_shift(comm, 0, 1, &left, &right);
    printf("[%d] Coordinates: %d\tNeighbor-Left: %d\tNeighbor-Right: %d\n", rank, *coordinates, left, right);


    return 0;

    srand(42 * 0x815);
    long n = 0;
    int tw = 0, th = 0, px = 0, py = 0;
    if(c > 1) n = atoi(v[1]);   ///< read timeSteps
    if (c > 2) tw = atoi(v[2]); ///< read thread-width
    if (c > 3) th = atoi(v[3]); ///< read thread-height
    if (c > 4) px = atoi(v[4]); ///< read thread-count X
    if (c > 5) py = atoi(v[5]); ///< read thread-count Y
    if(n <= 0) n = 100;         ///< default timeSteps
    if (tw <= 0) tw = 18;       ///< default thread-width
    if (th <= 0) th = 12;       ///< default thread-height
    if (px <= 0) px = 1;        ///< default thread-count X
    if (py <= 0) py = 1;        ///< default thread-count Y

    game(n, tw, th, px, py);
}
