# Abgabe Aufgabe Open-MP

## Ausführung Code

Um alle eventuell vorhandenen vtk-Dateien oder binaries zu löschen und das programm neu zu erstellen:\
`make clean && make`

Ausführung des Codes\
`./gameoflife <time-steps> <thread-width> <thread-height> <thread-count-w> <thread-count-h>`

Ausführung der Performance-Auswertung:\
` cd performance && python3 performance_analysis.py  <time-steps> <iterations-per-size> `

---