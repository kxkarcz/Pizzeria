import tkinter as tk
from tkinter import PhotoImage
import subprocess
import signal
import os
import threading
import time

class PizzeriaSimulation:
    """
    Klasa odpowiedzialna za zarządzanie symulacją pizzerii, w tym uruchamianie, zatrzymywanie procesu
    oraz obsługę sygnałów.
    """
    def __init__(self, update_callback, clear_console_callback):
        """
        Inicjalizuje symulację pizzerii.

        :param update_callback: Funkcja wywoływana do aktualizacji konsoli.
        :param clear_console_callback: Funkcja wywoływana do czyszczenia konsoli.
        """
        self.process = None
        self.console_output = ""
        self.initial_directory = os.getcwd()
        self.stop_thread = False
        self.auto_scroll = True
        self.firefighter_pid = None
        self.log_thread = None
        self.update_callback = update_callback
        self.clear_console_callback = clear_console_callback
        self.is_running = False

    def update_console(self, text):
        """
        Aktualizuje zawartość konsoli.

        :param text: Tekst do wyświetlenia w konsoli.
        """
        self.console_output += text + '\n'
        self.update_callback(text)

        if "Strażak uruchomiony. PID:" in text:
            self.firefighter_pid = int(text.split("PID:")[1].split(".")[0].strip())
            self.update_console(f"Znaleziono PID strażaka: {self.firefighter_pid}")

    def follow_log_file(self, file_path):
        """
        Śledzi plik logów i aktualizuje konsolę o nowe wpisy.

        :param file_path: Ścieżka do pliku logów.
        """
        with open(file_path, 'r') as file:
            file.seek(0, os.SEEK_END)
            while not self.stop_thread:
                line = file.readline()
                if not line:
                    time.sleep(0.1)
                    continue
                self.update_console(line.strip())

    def start_simulation(self):
        """
        Uruchamia symulację pizzerii.
        """
        if self.is_running:
            self.update_console("Symulacja już jest uruchomiona.")
            return

        self.stop_simulation()
        self.console_output = ""
        self.clear_console_callback()
        self.update_console("")
        self.firefighter_pid = None

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
            self.log_thread = threading.Thread(target=self.follow_log_file, args=(os.path.join(executable_dir, "logs.txt"),))
            self.log_thread.start()
            self.is_running = True
            self.update_console("Symulacja uruchomiona...")
        except Exception as e:
            self.update_console(f"Wystąpił błąd przy uruchamianiu procesu: {str(e)}")

    def send_fire_signal(self):
        """
        Wysyła sygnał do procesu strażaka, aby zainicjować ewakuację.
        """
        if self.firefighter_pid is not None:
            try:
                os.kill(self.firefighter_pid, signal.SIGUSR1)
                self.update_console(f"Wysłano sygnał do strażaka (PID: {self.firefighter_pid}).")
            except Exception as e:
                self.update_console(f"Nie udało się wysłać sygnału do strażaka: {str(e)}")
        else:
            self.update_console("Nie znaleziono PID strażaka. Nie można wysłać sygnału.")

    def stop_simulation(self):
        """
        Zatrzymuje symulację pizzerii.
        """
        if self.process is not None:
            self.process.terminate()
            self.process.wait()
            self.process = None

        self.stop_thread = True
        if self.log_thread and self.log_thread.is_alive():
            self.log_thread.join()
        self.log_thread = None
        self.is_running = False
        self.update_console("Symulacja zakończona.")


class Application(tk.Tk):
    """
    Główna klasa aplikacji GUI dla symulacji pizzerii.
    """
    def __init__(self):
        super().__init__()

        self.title("Symulacja Pizzerii")
        self.geometry("600x400")

        try:
            self.background_image = PhotoImage(file="background.png")
        except Exception:
            self.background_image = None
            print("Nie udało się załadować tła.")

        self.canvas = tk.Canvas(self, width=600, height=400)
        self.canvas.pack(fill="both", expand=True)

        if self.background_image:
            self.canvas.create_image(0, 0, image=self.background_image, anchor="nw")

        self.label = tk.Label(self, text="Pizzeria", bg="white", fg="black", font=("Helvetica", 24, "bold"))
        self.canvas.create_window(300, 50, window=self.label)

        self.output_text_frame = tk.Frame(self)
        self.output_text_widget = tk.Text(self.output_text_frame, wrap=tk.WORD, width=70, height=15, fg="black", bg="white", state=tk.DISABLED)
        self.scrollbar = tk.Scrollbar(self.output_text_frame, command=self.output_text_widget.yview)
        self.output_text_widget.configure(yscrollcommand=self.scrollbar.set)
        self.canvas.create_window(300, 200, window=self.output_text_frame)
        self.output_text_widget.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.scrollbar.pack(side=tk.RIGHT, fill=tk.Y)

        self.output_text_widget.bind("<MouseWheel>", self.on_scroll)
        self.output_text_widget.bind("<Button-4>", self.on_scroll)
        self.output_text_widget.bind("<Button-5>", self.on_scroll)

        self.simulation = PizzeriaSimulation(self.update_console, self.clear_console)

        self.create_buttons()

    def update_console(self, message):
        """
        Aktualizuje konsolę GUI o nowy komunikat.

        :param message: Wiadomość do wyświetlenia.
        """
        self.output_text_widget.config(state=tk.NORMAL)
        self.output_text_widget.insert(tk.END, message + '\n')
        if self.simulation.auto_scroll:
            self.output_text_widget.yview(tk.END)
        self.output_text_widget.config(state=tk.DISABLED)

    def clear_console(self):
        """
        Czyści konsolę GUI.
        """
        self.output_text_widget.config(state=tk.NORMAL)
        self.output_text_widget.delete(1.0, tk.END)
        self.output_text_widget.config(state=tk.DISABLED)

    def stop_and_quit(self):
        """
        Zatrzymuje symulację i zamyka aplikację.
        """
        self.simulation.stop_simulation()
        self.quit()

    def create_buttons(self):
        """
        Tworzy przyciski sterujące w GUI.
        """
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

    def on_scroll(self, event):
        """
        Wyłącza automatyczne przewijanie, gdy użytkownik ręcznie przewija konsolę.

        :param event: Wydarzenie przewijania.
        """
        self.simulation.auto_scroll = False


if __name__ == "__main__":
    app = Application()
    app.mainloop()