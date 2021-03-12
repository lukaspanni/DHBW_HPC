#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mpi/mpi.h>

#define calcIndex(width, x, y)  ((y)*(width) + (x))

int rank;
int right_neighbor, left_neighbor, top_neighbor, bottom_neighbor;

#define performance

void writeVTK2(long timestep, const double *data, char prefix[1024], int processWidth, int processHeight, int offsetX, int offsetY) {
    char filename[2048];
    int x, y;

    float deltax = 1.0;
    long nxy = processWidth * processHeight * sizeof(float);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    snprintf(filename, sizeof(filename), "%s-%05ld-%03d%s", prefix, timestep, rank, ".vti");
    FILE *fp = fopen(filename, "w");

    fprintf(fp, "<?xml version=\"1.0\"?>\n");
    fprintf(fp, "<VTKFile type=\"ImageData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n");
    fprintf(fp, "<ImageData WholeExtent=\"%d %d %d %d %d %d\" Origin=\"0 0 0\" Spacing=\"%le %le %le\">\n", offsetX,
            offsetX + processWidth - 2, offsetY, offsetY + processHeight - 2, 0, 0, deltax, deltax, 0.0);
    fprintf(fp, "<CellData Scalars=\"%s\">\n", prefix);
    fprintf(fp, "<DataArray type=\"Float32\" Name=\"%s\" format=\"appended\" offset=\"0\"/>\n", prefix);
    fprintf(fp, "</CellData>\n");
    fprintf(fp, "</ImageData>\n");
    fprintf(fp, "<AppendedData encoding=\"raw\">\n");
    fprintf(fp, "_");
    fwrite((unsigned char *) &nxy, sizeof(long), 1, fp);

    // start at 1 and end -1 -> ghost layer
    for (y = 1; y < processHeight-1; y++) {
        for (x = 1; x < processWidth-1; x++) {
            float value = data[calcIndex(processWidth, x, y)];
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


void evolve(int timestep, double *currentfield, double *newfield, int w, int h, MPI_Comm* comm) {

        int x, y;

        //printf("THREAD: %i with offset (%i | %i)\n", this_thread, offsetX, offsetY);
        for (y = 1; y < h; y++) {
            for (x = 1; x < w; x++) {
                int n = countLivingsPeriodic(currentfield, x, y, w, h);
                int index = calcIndex(w, x, y);
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
        int coordinates[2];
        MPI_Cart_coords(*comm, rank, 2, coordinates);
        writeVTK2(timestep, currentfield, "gol", w, h, coordinates[1]*(w - 2), coordinates[0]*(h-2));
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

void game(MPI_Comm* comm, long timeSteps, int tw, int th, int px, int py) {
    int h,w;
    h = th+2;
    w = tw+2;

    int size_arrays[2] = {h, w};
    int size_subarrays_vert[2] = {h, 1};
    int size_subarrays_hori[2] = {1, w};

    MPI_Datatype ghTop;
    MPI_Datatype ghBottom;
    MPI_Datatype ghRight;
    MPI_Datatype ghLeft;

    MPI_Datatype innerTop;
    MPI_Datatype innerBottom;
    MPI_Datatype innerRight;
    MPI_Datatype innerLeft;

    int n = 2;
    int start_indices[2] = {0, 0};
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_vert, start_indices, MPI_ORDER_C, MPI_DOUBLE, &ghLeft);
    start_indices[1] = 1;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_vert, start_indices, MPI_ORDER_C, MPI_DOUBLE, &innerLeft);
    start_indices[1] = w - 1;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_vert, start_indices, MPI_ORDER_C, MPI_DOUBLE, &ghRight);
    start_indices[1] = w - 2;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_vert, start_indices, MPI_ORDER_C, MPI_DOUBLE, &innerRight);

    MPI_Type_commit(&ghLeft);
    MPI_Type_commit(&innerLeft);
    MPI_Type_commit(&ghRight);
    MPI_Type_commit(&innerRight);

    start_indices[0] = 0;
    start_indices[1] = 0;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_hori, start_indices, MPI_ORDER_C, MPI_DOUBLE, &ghTop);
    start_indices[0] = 1;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_hori, start_indices, MPI_ORDER_C, MPI_DOUBLE, &innerTop);
    start_indices[0] = h - 1;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_hori, start_indices, MPI_ORDER_C, MPI_DOUBLE, &ghBottom);
    start_indices[0] = h - 2;
    MPI_Type_create_subarray(n, size_arrays, size_subarrays_hori, start_indices, MPI_ORDER_C, MPI_DOUBLE, &innerBottom);


    MPI_Type_commit(&ghTop);
    MPI_Type_commit(&innerTop);
    MPI_Type_commit(&ghBottom);
    MPI_Type_commit(&innerBottom);


    // if(rank == 0){
    //     // load file / random init in one process
    //     int w, h;
    //     w = tw * px;
    //     h = th * py;
    //     double *loadedfield = calloc(w * h, sizeof(double));
    //     filling(loadedfield, w, h, "file.rle");

    //     free(loadedfield);
    // }
    // else{
    //     MPI_Status status;
    //     MPI_Recv(currentfield, w * h, MPI_DOUBLE, 0, rank, *comm, status);
    // }
    

    double *currentfield = calloc(w * h, sizeof(double));
    double *newfield = calloc(w * h, sizeof(double));

    //Also fills ghost-layer, will be overwritten
    fillRandom(currentfield, w, h);


    //-> arrays of request/status for async communication
    MPI_Request request[8];
    MPI_Status status[8];

    long t;
    for (t = 0; t < timeSteps; t++) {
        MPI_Isend(currentfield, 1, innerLeft, left_neighbor, 1, *comm, request + 0);
        MPI_Irecv(currentfield, 1, ghRight, right_neighbor, 1, *comm, request + 1);
        MPI_Irecv(currentfield, 1, ghLeft, left_neighbor, 2, *comm, request + 2);
        MPI_Isend(currentfield, 1, innerRight, right_neighbor, 2, *comm, request + 3);

        MPI_Isend(currentfield, 1, innerTop, top_neighbor, 3, *comm, request + 4);
        MPI_Irecv(currentfield, 1, ghBottom, bottom_neighbor, 3, *comm, request + 5);
        MPI_Irecv(currentfield, 1, ghTop, top_neighbor, 4, *comm, request + 6);
        MPI_Isend(currentfield, 1, innerBottom, bottom_neighbor, 4, *comm, request + 7);

        MPI_Waitall(8, request, status);

        evolve(t, currentfield, newfield, w, h, comm);

#ifndef performance
        if(rank == 0) {
            writeVTK2_parallel(t, "golp", "gol", tw*px, th*py, px, py);
            printf("%ld timestep\n", t);
        }

#endif

        int equal = 1;
        for(int i = 0; i < w*h; i++){
            if(currentfield[i] != newfield[i]){
                equal = 0;
                break;
            }
        }
        int reduced_equal;
        MPI_Allreduce(&equal, &reduced_equal, 1, MPI_INT, MPI_LAND, *comm);
        if(reduced_equal){
            break;
        }
        //SWAP
        double *temp = currentfield;
        currentfield = newfield;
        newfield = temp;
    }

    free(currentfield);
    free(newfield);

}

int main(int c, char **v) {
    MPI_Init(&c, &v);

    srand(42 * 0x815);
    long n = 0;
    int tw = 0, th = 0, px = 0, py = 0;
    if(c > 1) n = atoi(v[1]);   ///< read timeSteps
    if (c > 2) tw = atoi(v[2]); ///< read process-width
    if (c > 3) th = atoi(v[3]); ///< read process-height
    if (c > 4) px = atoi(v[4]); ///< read process-count X
    if (c > 5) py = atoi(v[5]); ///< read process-count Y
    if(n <= 0) n = 100;         ///< default timeSteps
    if (tw <= 0) tw = 10;       ///< default process-width
    if (th <= 0) th = 10;       ///< default process-height
    if (px <= 0) px = 2;        ///< default process-count X
    if (py <= 0) py = 2;        ///< default process-count Y

    int commSize;

    MPI_Comm_size(MPI_COMM_WORLD, &commSize);
    if(commSize != px*py){
        printf("ERROR Comm-Size != px*py\n");
        return -1;
    }
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

#ifndef performance
    printf("Initialized, Size: %d, Rank: %d\n", commSize, rank);
#endif

    MPI_Comm comm;
    int dimensions[] = {px, py};
    int periodic[] = {1, 1};
    MPI_Cart_create(MPI_COMM_WORLD, 2, dimensions, periodic, 0, &comm);

    int coordinates[2];
    MPI_Cart_coords(comm, rank, 2, coordinates);

    MPI_Cart_shift(comm, 1, 1, &left_neighbor, &right_neighbor);
    MPI_Cart_shift(comm, 0, 1, &top_neighbor, &bottom_neighbor);

#ifndef performance
    printf("[%d] Coordinates: (%d|%d)\tNeighbor-Left: %d\tNeighbor-Right: %d\tNeighbor-Top: %d\tNeighbor-Bottom: %d\n", rank, *coordinates, *(coordinates+1), left_neighbor, right_neighbor, top_neighbor, bottom_neighbor);
#endif

    game(&comm, n, tw, th, px, py);

    MPI_Finalize();
}