# Abgabe Aufgabe MPI

## Ausführung Code

Um alle eventuell vorhandenen vtk-Dateien oder binaries zu löschen und das programm neu zu erstellen:\
`make clean && make`

Ausführung des Codes\
`mpirun -n <px*py> ./gameoflife <time-steps> <process-width> <process-height> <px> <py>`

Ausführung der Performance-Auswertung:\
` cd performance && python3 performance_analysis.py  <time-steps> <iterations-per-size> `

---
