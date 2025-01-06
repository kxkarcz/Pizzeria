import tkinter as tk
from tkinter import PhotoImage
import subprocess
import signal
import os
import threading
import time

class PizzeriaSimulation:
    def __init__(self, output_text_widget):
        self.process = None
        self.console_output = ""
        self.output_text_widget = output_text_widget
        self.initial_directory = os.getcwd()
        self.stop_thread = False
        self.auto_scroll = True

        self.output_text_widget.bind("<MouseWheel>", self.on_user_scroll)
        self.output_text_widget.bind("<Button-4>", self.on_user_scroll)
        self.output_text_widget.bind("<Button-5>", self.on_user_scroll)
        self.output_text_widget.bind("<KeyPress>", self.on_user_scroll)

    def on_user_scroll(self, event):
        self.auto_scroll = False
        if self.output_text_widget.yview()[1] == 1.0:
            self.auto_scroll = True

    def update_console(self, text):
        self.console_output += text + '\n'
        self.output_text_widget.config(state=tk.NORMAL)
        self.output_text_widget.insert(tk.END, text + '\n')

        if self.auto_scroll:
            self.output_text_widget.yview(tk.END)
        self.output_text_widget.config(state=tk.DISABLED)


    def follow_log_file(self, file_path):
        with open(file_path, 'r') as file:
            file.seek(0, os.SEEK_END)
            while not self.stop_thread:
                line = file.readline()
                if not line:
                    time.sleep(0.1)
                    continue
                self.update_console(line.strip())

    def start_simulation(self):
        self.console_output = ""
        self.update_console("")

        executable_dir = os.path.join(self.initial_directory, "../../cmake-build-debug")
        executable_name = "./Pizzeria"

        if not os.path.isdir(executable_dir):
            self.update_console(f"Nie znaleziono folderu: {executable_dir}")
            return

        os.chdir(executable_dir)
        try:
            self.process = subprocess.Popen(
                executable_name,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            self.stop_thread = False
            threading.Thread(target=self.follow_log_file, args=(os.path.join(executable_dir, "logs.txt"),)).start()
            self.update_console("Symulacja uruchomiona...")
        except Exception as e:
            self.update_console(f"Wystąpił błąd przy uruchamianiu procesu: {str(e)}")

    def send_fire_signal(self):
        if self.process is not None:
            try:
                os.kill(self.process.pid, signal.SIGTTOU)
                self.update_console("Wysłano sygnał strażaka.")
            except Exception as e:
                self.update_console(f"Nie udało się wysłać sygnału strażaka: {str(e)}")
        else:
            self.update_console("Proces nie jest uruchomiony.")

    def stop_simulation(self):
        if self.process is not None:
            self.process.terminate()
            self.update_console("Symulacja zakończona.")
            self.stop_thread = True
        else:
            self.update_console("Proces nie jest uruchomiony.")

class Application(tk.Tk):
    def __init__(self):
        super().__init__()

        self.title("Symulacja Pizzerii")
        self.geometry("600x400")
        self.background_image = PhotoImage(file="background.png")

        self.canvas = tk.Canvas(self, width=600, height=400)
        self.canvas.pack(fill="both", expand=True)
        self.canvas.create_image(0, 0, image=self.background_image, anchor="nw")

        self.label = tk.Label(self, text="Pizzeria", bg="white", fg="black", font=("Helvetica", 24, "bold"))
        self.canvas.create_window(300, 50, window=self.label)

        self.output_text_widget = tk.Text(self, wrap=tk.WORD, width=70, height=15, fg="black", bg="white", state=tk.DISABLED)
        self.canvas.create_window(300, 200, window=self.output_text_widget)

        self.simulation = PizzeriaSimulation(self.output_text_widget)

        self.create_buttons()

    def stop_and_quit(self):
        self.simulation.stop_simulation()
        self.quit()

    def create_buttons(self):
        button_bg = "white"

        start_button = tk.Button(self, text="Start Symulacji", command=self.simulation.start_simulation,
                                 bg=button_bg, highlightthickness=0, bd=0)
        fire_button = tk.Button(self, text="Strażak", command=self.simulation.send_fire_signal,
                                bg=button_bg, highlightthickness=0, bd=0)
        stop_button = tk.Button(self, text="Zakończ", command=self.stop_and_quit,
                                bg=button_bg, highlightthickness=0, bd=0)

        self.canvas.create_window(150, 350, window=start_button)
        self.canvas.create_window(300, 350, window=fire_button)
        self.canvas.create_window(450, 350, window=stop_button)

app = Application()
app.mainloop()