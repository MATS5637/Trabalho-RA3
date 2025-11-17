#!/usr/bin/env python3
import os
import sys
import csv
from pathlib import Path

import matplotlib.pyplot as plt

DOCS = Path("docs")

def ler_csv(caminho):
    with open(caminho, newline="") as f:
        reader = csv.DictReader(f)
        linhas = list(reader)
    return linhas

def plot_cpu_completo():
    cpu_rm = DOCS / "cpu_rm.csv"
    cpu_ps = DOCS / "cpu_ps.csv"
    if not cpu_rm.exists() or not cpu_ps.exists():
        print("Preciso de docs/cpu_rm.csv e docs/cpu_ps.csv. Rode compare_tools.sh antes.")
        return

    rm = ler_csv(cpu_rm)
    ps = ler_csv(cpu_ps)

    x_rm = [int(l["amostra"]) for l in rm]
    y_rm_proc = [float(l["cpu_processo_percent"]) for l in rm]
    y_rm_sys = [float(l["cpu_sistema_percent"]) for l in rm]

    x_ps = [int(l["amostra"]) for l in ps]
    y_ps_cpu = [float(l["cpu_percent"]) for l in ps]

    plt.figure()
    plt.plot(x_rm, y_rm_proc, label="RM - CPU processo")
    plt.plot(x_rm, y_rm_sys, label="RM - CPU sistema")
    plt.plot(x_ps, y_ps_cpu, label="ps - %CPU")
    plt.xlabel("Amostra")
    plt.ylabel("CPU (%)")
    plt.legend()
    plt.title("Comparação de CPU: resource-monitor vs ps")
    plt.grid(True)
    plt.tight_layout()
    plt.show()

def plot_mem_completo():
    mem_rm = DOCS / "mem_rm.csv"
    mem_ps = DOCS / "mem_ps.csv"
    if not mem_rm.exists() or not mem_ps.exists():
        print("Preciso de docs/mem_rm.csv e docs/mem_ps.csv. Rode compare_tools.sh antes.")
        return

    rm = ler_csv(mem_rm)
    ps = ler_csv(mem_ps)

    x_rm = [int(l["amostra"]) for l in rm]
    y_rm_pct = [float(l["proc_mem_percent"]) for l in rm]

    x_ps = [int(l["amostra"]) for l in ps]
    y_ps_pct = [float(l["mem_percent"]) for l in ps]

    plt.figure()
    plt.plot(x_rm, y_rm_pct, label="RM - %Mem processo")
    plt.plot(x_ps, y_ps_pct, label="ps - %MEM")
    plt.xlabel("Amostra")
    plt.ylabel("Memória (%)")
    plt.legend()
    plt.title("Comparação de Memória: resource-monitor vs ps")
    plt.grid(True)
    plt.tight_layout()
    plt.show()

def plot_io_rm():
    io_rm = DOCS / "io_rm.csv"
    if not io_rm.exists():
        print("Preciso de docs/io_rm.csv. Rode compare_tools.sh antes.")
        return

    rm = ler_csv(io_rm)

    x = [int(l["amostra"]) for l in rm]
    read_bps = [float(l["read_bps"]) for l in rm]
    write_bps = [float(l["write_bps"]) for l in rm]

    plt.figure()
    plt.plot(x, read_bps, label="read_bps")
    plt.plot(x, write_bps, label="write_bps")
    plt.xlabel("Amostra")
    plt.ylabel("Bytes/s")
    plt.legend()
    plt.title("I/O (resource-monitor)")
    plt.grid(True)
    plt.tight_layout()
    plt.show()

def main():
    print("Selecione o gráfico:")
    print("1) CPU completo (RM vs ps)")
    print("2) Memória completa (RM vs ps)")
    print("3) I/O (RM)")
    escolha = input("Opção: ").strip()

    if escolha == "1":
        plot_cpu_completo()
    elif escolha == "2":
        plot_mem_completo()
    elif escolha == "3":
        plot_io_rm()
    else:
        print("Opção inválida.")

if __name__ == "__main__":
    main()
