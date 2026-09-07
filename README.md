# Operative-System-Project

# 📮 Post Office OS Simulator
![C](https://img.shields.io/badge/c-%2300599C.svg?style=for-the-badge&logo=c&logoColor=white) ![Linux](https://img.shields.io/badge/Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)

## 📖 About
Questo progetto in C simula il funzionamento di un ufficio postale, implementando logiche di concorrenza, sincronizzazione e comunicazione tra processi (IPC) basate sulle API di Linux. È un'ottima dimostrazione pratica di come gestire flussi multipli in un ambiente multi-processo e multi-thread, garantendo robustezza e corretta gestione della memoria.

## ✨ Features principali
- **Simulazione Realistica:** Gestione concorrente di clienti, sportelli (worker), erogatore di biglietti (ticket dispenser) e un direttore.
- **Inter-Process Communication (IPC):** Sfrutta code di messaggi, semafori e memoria condivisa per coordinare i processi in modo sicuro e prevenire deadlock.
- **Memory & Resource Management:** Attenta gestione della memoria dinamica e pulizia sicura delle risorse di sistema.

## 🛠 Tech Stack
- **Linguaggio:** C
- **OS/API:** Linux POSIX
- **Tools:** GCC, Makefile, Valgrind, GDB

## 🚀 Getting Started
Segui questi step per compilare ed eseguire il progetto localmente:

1. Clona la repository:
   ```bash
   git clone <URL-DELLA-TUA-REPO>
   cd Operative-System-Project-main
   ```
2. Compila il progetto utilizzando Make:
   ```bash
   make all
   ```
3. Avvia la simulazione principale:
   ```bash
   make run
   ```
*(Nota: puoi testare la gestione della memoria con `make valgrind` e ripristinare/pulire l'ambiente IPC con `make clean`)*
