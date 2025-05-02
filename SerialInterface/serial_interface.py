import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import datetime
import os
import json

class SerialMonitorApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Monitor Serial STM32")
        self.root.geometry("850x600")
        self.root.configure(bg="#f0f0f0")
        self.root.iconbitmap(self.resource_path("icon.ico")) if os.path.exists(self.resource_path("icon.ico")) else None
        
        # Tema personalizado
        self.style = ttk.Style()
        self.style.theme_use('clam')
        self.style.configure("TButton", background="#4a86e8", foreground="white", font=('Arial', 10, 'bold'))
        self.style.configure("TLabel", background="#f0f0f0", font=('Arial', 10))
        self.style.configure("TCombobox", background="#f0f0f0", font=('Arial', 10))
        self.style.configure("TFrame", background="#f0f0f0")
        
        # Variables
        self.serial_port = None
        self.is_connected = False
        self.is_monitoring = False
        self.monitor_thread = None
        
        # Cargar configuración
        self.config = self.load_config()
        
        # Frame principal
        self.main_frame = ttk.Frame(self.root, padding="10")
        self.main_frame.pack(fill=tk.BOTH, expand=True)
        
        # Panel superior - Configuración de conexión
        self.connection_frame = ttk.LabelFrame(self.main_frame, text="Configuración de Conexión", padding="10")
        self.connection_frame.pack(fill=tk.X, pady=5)
        
        # Fila 1 - Selección de puerto COM y velocidad
        row1 = ttk.Frame(self.connection_frame)
        row1.pack(fill=tk.X, pady=5)
        
        ttk.Label(row1, text="Puerto:").pack(side=tk.LEFT, padx=5)
        self.port_combobox = ttk.Combobox(row1, width=15)
        self.port_combobox.pack(side=tk.LEFT, padx=5)
        
        ttk.Button(row1, text="⟳", width=3, command=self.refresh_ports).pack(side=tk.LEFT, padx=2)
        
        ttk.Label(row1, text="Baudrate:").pack(side=tk.LEFT, padx=5)
        self.baudrate_combobox = ttk.Combobox(row1, width=10, values=["9600", "28800", "38400", "57600", "115200", "921600"])
        self.baudrate_combobox.pack(side=tk.LEFT, padx=5)
        self.baudrate_combobox.set(self.config.get("baudrate", "28800"))
        
        # Fila 2 - Otros parámetros
        row2 = ttk.Frame(self.connection_frame)
        row2.pack(fill=tk.X, pady=5)
        
        ttk.Label(row2, text="Data Bits:").pack(side=tk.LEFT, padx=5)
        self.databits_combobox = ttk.Combobox(row2, width=5, values=["5", "6", "7", "8"])
        self.databits_combobox.pack(side=tk.LEFT, padx=5)
        self.databits_combobox.set(self.config.get("databits", "8"))
        
        ttk.Label(row2, text="Paridad:").pack(side=tk.LEFT, padx=5)
        self.parity_combobox = ttk.Combobox(row2, width=5, values=["N", "E", "O", "M", "S"])
        self.parity_combobox.pack(side=tk.LEFT, padx=5)
        self.parity_combobox.set(self.config.get("parity", "N"))
        
        ttk.Label(row2, text="Stop Bits:").pack(side=tk.LEFT, padx=5)
        self.stopbits_combobox = ttk.Combobox(row2, width=5, values=["1", "1.5", "2"])
        self.stopbits_combobox.pack(side=tk.LEFT, padx=5)
        self.stopbits_combobox.set(self.config.get("stopbits", "1"))
        
        # Botones de control
        buttons_frame = ttk.Frame(row2)
        buttons_frame.pack(side=tk.RIGHT, padx=5)
        
        self.connect_button = ttk.Button(buttons_frame, text="Conectar", command=self.toggle_connection)
        self.connect_button.pack(side=tk.LEFT, padx=5)
        
        self.clear_button = ttk.Button(buttons_frame, text="Limpiar", command=self.clear_output)
        self.clear_button.pack(side=tk.LEFT, padx=5)
        
        # Panel central - Área de mensajes
        self.message_frame = ttk.LabelFrame(self.main_frame, text="Mensajes", padding="10")
        self.message_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        # Área de texto para mostrar mensajes recibidos
        self.output_text = scrolledtext.ScrolledText(self.message_frame, wrap=tk.WORD, height=12, background="#ffffff")
        self.output_text.pack(fill=tk.BOTH, expand=True, pady=5)
        self.output_text.config(state=tk.DISABLED)
        
        # Panel inferior - Envío de mensajes
        self.input_frame = ttk.LabelFrame(self.main_frame, text="Enviar Mensaje", padding="10")
        self.input_frame.pack(fill=tk.X, pady=5)
        
        # Entrada de texto para enviar
        self.input_text = tk.Text(self.input_frame, wrap=tk.WORD, height=4, background="#ffffff")
        self.input_text.pack(fill=tk.X, pady=5)
        self.input_text.bind("<KeyPress>", self.update_char_count)
        
        # Fila de contador de caracteres y botón de envío
        bottom_row = ttk.Frame(self.input_frame)
        bottom_row.pack(fill=tk.X, pady=5)
        
        self.char_count_label = ttk.Label(bottom_row, text="0/16 caracteres")
        self.char_count_label.pack(side=tk.LEFT, padx=5)
        
        self.lcd_preview_button = ttk.Button(bottom_row, text="Vista previa LCD", command=self.show_lcd_preview)
        self.lcd_preview_button.pack(side=tk.LEFT, padx=5)
        
        self.send_button = ttk.Button(bottom_row, text="Enviar Mensaje", command=self.send_message)
        self.send_button.pack(side=tk.RIGHT, padx=5)
        
        # Barra de estado
        self.status_frame = ttk.Frame(self.root, relief=tk.SUNKEN)
        self.status_frame.pack(fill=tk.X, side=tk.BOTTOM)
        
        self.status_label = ttk.Label(self.status_frame, text="Desconectado", anchor=tk.W)
        self.status_label.pack(side=tk.LEFT, fill=tk.X, padx=5)
        
        # Información de autor
        self.author_label = ttk.Label(self.status_frame, text="Universidad Militar Nueva Granada - Lab. Microcontroladores 2025", anchor=tk.E)
        self.author_label.pack(side=tk.RIGHT, padx=5)
        
        # Inicializar
        self.refresh_ports()
        self.update_ui_state()
        
    def resource_path(self, relative_path):
        """ Get absolute path to resource, works for dev and for PyInstaller """
        try:
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")
        return os.path.join(base_path, relative_path)
    
    def load_config(self):
        """Cargar configuración desde archivo JSON"""
        config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
        if os.path.exists(config_path):
            try:
                with open(config_path, 'r') as f:
                    return json.load(f)
            except:
                pass
        return {"baudrate": "115200", "databits": "8", "parity": "N", "stopbits": "1", "last_port": ""}
    
    def save_config(self):
        """Guardar configuración en archivo JSON"""
        config = {
            "baudrate": self.baudrate_combobox.get(),
            "databits": self.databits_combobox.get(),
            "parity": self.parity_combobox.get(),
            "stopbits": self.stopbits_combobox.get(),
            "last_port": self.port_combobox.get()
        }
        config_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "config.json")
        try:
            with open(config_path, 'w') as f:
                json.dump(config, f)
        except:
            pass
    
    def refresh_ports(self):
        """Refrescar lista de puertos COM disponibles"""
        ports = [port.device for port in serial.tools.list_ports.comports()]
        self.port_combobox['values'] = ports
        
        # Seleccionar el último puerto usado o el primero disponible
        if self.config.get("last_port") in ports:
            self.port_combobox.set(self.config.get("last_port"))
        elif ports:
            self.port_combobox.set(ports[0])
    
    def toggle_connection(self):
        """Conectar o desconectar del puerto serial"""
        if not self.is_connected:
            self.connect()
        else:
            self.disconnect()
    
    def connect(self):
        """Conectar al puerto serial"""
        port = self.port_combobox.get()
        baudrate = int(self.baudrate_combobox.get())
        databits = int(self.databits_combobox.get())
        parity = self.parity_combobox.get()
        stopbits = float(self.stopbits_combobox.get())
        
        if not port:
            messagebox.showerror("Error", "Selecciona un puerto COM")
            return
        
        try:
            self.serial_port = serial.Serial(
                port=port,
                baudrate=baudrate,
                bytesize=databits,
                parity=parity,
                stopbits=stopbits,
                timeout=0.1
            )
            self.is_connected = True
            self.is_monitoring = True
            
            # Iniciar thread para recibir datos
            self.monitor_thread = threading.Thread(target=self.monitor_serial)
            self.monitor_thread.daemon = True
            self.monitor_thread.start()
            
            self.save_config()
            self.update_ui_state()
            self.log_message(f"Conectado a {port} a {baudrate} baudios")
            
        except Exception as e:
            messagebox.showerror("Error de conexión", f"No se pudo conectar al puerto {port}: {str(e)}")
    
    def disconnect(self):
        """Desconectar del puerto serial"""
        if self.serial_port:
            self.is_monitoring = False
            if self.monitor_thread:
                self.monitor_thread.join(timeout=1.0)
            
            self.serial_port.close()
            self.serial_port = None
            self.is_connected = False
            
            self.update_ui_state()
            self.log_message("Desconectado")
    
    def monitor_serial(self):
        """Thread para monitorear datos recibidos del puerto serial"""
        buffer = b""
        while self.is_monitoring:
            if self.serial_port and self.serial_port.is_open:
                try:
                    # Leer datos disponibles
                    data = self.serial_port.read(100)
                    if data:
                        buffer += data
                        
                        try:
                            # Intentar decodificar el buffer
                            decoded_data = buffer.decode('latin-1')
                            self.log_message(f"RX: {decoded_data}", is_receive=True)
                            buffer = b""  # Limpiar buffer después de procesar
                        except UnicodeDecodeError:
                            # Si no se puede decodificar, esperar más datos
                            pass
                            
                except Exception as e:
                    self.log_message(f"Error de lectura: {str(e)}")
                    self.disconnect()
                    break
            
            time.sleep(0.01)
    
    def send_message(self):
        """Enviar mensaje al microcontrolador"""
        if not self.is_connected or not self.serial_port:
            messagebox.showwarning("No conectado", "Conecta primero al puerto serial")
            return
        
        message = self.input_text.get("1.0", tk.END).strip()
        if not message:
            return
        
        try:
            self.serial_port.write(message.encode('latin-1'))
            self.log_message(f"TX: {message}", is_send=True)
            
            # Mostrar vista previa de cómo se verá en el LCD
            self.show_lcd_preview()
            
            # Limpiar área de entrada después de enviar
            self.input_text.delete("1.0", tk.END)
            self.update_char_count(None)
            
        except Exception as e:
            messagebox.showerror("Error de envío", f"No se pudo enviar el mensaje: {str(e)}")
    
    def show_lcd_preview(self):
        """Mostrar vista previa de cómo se verá el mensaje en la LCD"""
        message = self.input_text.get("1.0", tk.END).strip()
        if not message:
            return
        
        # Crear ventana de vista previa
        preview = tk.Toplevel(self.root)
        preview.title("Vista Previa LCD 16x2")
        preview.geometry("350x150")
        preview.resizable(False, False)
        
        # Crear frame para simular LCD
        lcd_frame = tk.Frame(preview, bg="blue", bd=2, relief=tk.RAISED, padx=10, pady=10)
        lcd_frame.pack(expand=True, fill=tk.BOTH, padx=20, pady=20)
        
        # Línea 1
        line1 = message[:16]
        line1_display = tk.Label(lcd_frame, text=line1, font=('Courier', 12), bg="#2c3e50", fg="#7FFFD4",
                                anchor=tk.W, width=16, height=1, padx=5, pady=5)
        line1_display.pack(fill=tk.X)
        
        # Línea 2 (si el mensaje es más largo que 16 caracteres)
        line2 = message[16:32] if len(message) > 16 else ""
        line2_display = tk.Label(lcd_frame, text=line2, font=('Courier', 12), bg="#2c3e50", fg="#7FFFD4",
                                anchor=tk.W, width=16, height=1, padx=5, pady=5)
        line2_display.pack(fill=tk.X)
    
    def log_message(self, message, is_receive=False, is_send=False):
        """Mostrar mensaje en el área de salida"""
        timestamp = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        
        # Aplicar formato según el tipo de mensaje
        if is_receive:
            formatted_msg = f"[{timestamp}] → {message}\n"
            tag = "receive"
        elif is_send:
            formatted_msg = f"[{timestamp}] ← {message}\n"
            tag = "send"
        else:
            formatted_msg = f"[{timestamp}] {message}\n"
            tag = "system"
        
        # Actualizar área de texto
        self.output_text.config(state=tk.NORMAL)
        self.output_text.insert(tk.END, formatted_msg, tag)
        self.output_text.tag_configure("receive", foreground="green")
        self.output_text.tag_configure("send", foreground="blue")
        self.output_text.tag_configure("system", foreground="black")
        self.output_text.see(tk.END)  # Auto-scroll
        self.output_text.config(state=tk.DISABLED)
    
    def clear_output(self):
        """Limpiar área de salida"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete("1.0", tk.END)
        self.output_text.config(state=tk.DISABLED)
    
    def update_char_count(self, event):
        """Actualizar contador de caracteres"""
        text = self.input_text.get("1.0", tk.END).strip()
        count = len(text)
        
        if count <= 16:
            self.char_count_label.config(text=f"{count}/16 caracteres (1 línea)")
        else:
            self.char_count_label.config(text=f"{count}/32 caracteres (2 líneas)")
            
        # Limitar a 32 caracteres (2 líneas de LCD)
        if count > 32 and event and event.keysym not in ('BackSpace', 'Delete'):
            self.input_text.delete("1.0 + 32 chars", tk.END)
    
    def update_ui_state(self):
        """Actualizar estado de la interfaz según la conexión"""
        if self.is_connected:
            self.connect_button.config(text="Desconectar")
            self.status_label.config(text=f"Conectado a {self.port_combobox.get()}")
            self.send_button.config(state=tk.NORMAL)
            self.lcd_preview_button.config(state=tk.NORMAL)
            
            # Deshabilitar cambios en configuración mientras está conectado
            self.port_combobox.config(state="disabled")
            self.baudrate_combobox.config(state="disabled")
            self.databits_combobox.config(state="disabled")
            self.parity_combobox.config(state="disabled")
            self.stopbits_combobox.config(state="disabled")
        else:
            self.connect_button.config(text="Conectar")
            self.status_label.config(text="Desconectado")
            self.send_button.config(state=tk.DISABLED)
            self.lcd_preview_button.config(state=tk.DISABLED)
            
            # Habilitar cambios en configuración
            self.port_combobox.config(state="readonly")
            self.baudrate_combobox.config(state="readonly")
            self.databits_combobox.config(state="readonly")
            self.parity_combobox.config(state="readonly")
            self.stopbits_combobox.config(state="readonly")

if __name__ == "__main__":
    root = tk.Tk()
    app = SerialMonitorApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.disconnect(), root.destroy()))
    root.mainloop()