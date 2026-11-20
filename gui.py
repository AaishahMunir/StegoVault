"""
StegoVault GUI
--------------
A Tkinter front end for the existing StegoVault.exe (C program).

This file does NOT hide or encrypt anything itself. It only:
  1. collects input from the user,
  2. feeds that input to StegoVault.exe exactly as if it were typed into the
     console,
  3. reads the program's output and shows the result.

Place this file in the same folder as StegoVault.exe and run:  py gui.py
"""

import os
import subprocess
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from PIL import Image, ImageTk

# --------------------------------------------------------------------------
# Paths and limits (these mirror common.h in the C project)
# --------------------------------------------------------------------------
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
EXE_PATH = os.path.join(BASE_DIR, "StegoVault.exe")

MAX_MESSAGE_SIZE = 4096     # MAX_MESSAGE_SIZE in common.h
MAX_PASSWORD_SIZE = 128     # MAX_PASSWORD_SIZE in common.h
HEADER_BITS = 96            # STEGO_HEADER_BITS in common.h
MAX_PATH_BYTES = 511        # C buffers for filenames are 512 bytes

# --------------------------------------------------------------------------
# Colour palette and fonts
# --------------------------------------------------------------------------
BG = "#F5F7FB"
CARD = "#FFFFFF"
TEXT = "#172B4D"
MUTED = "#667085"
PRIMARY = "#2563EB"
PRIMARY_HOVER = "#1D4ED8"
BORDER = "#E4E8F0"
SUCCESS = "#16845B"
ERROR = "#C0392B"

FONT = "Segoe UI"

# --------------------------------------------------------------------------
# Texts printed by the C program (all ASCII, so we search them as bytes)
# --------------------------------------------------------------------------
ENCODE_OK = b"Message encrypted and hidden successfully!"
DECODE_OK = b"Hidden message recovered successfully!"
MESSAGE_LABEL = b"Message: "
MENU_MARKER = b"\n1. Encode / Hide Message"   # menu printed after the result

# Known C error texts -> friendly text shown to the user
KNOWN_ERRORS = [
    (b"Incorrect password or corrupted message",
     "Incorrect password or corrupted message."),
    (b"No hidden message found", "No hidden message found in this image."),
    (b"Unsupported BMP format",
     "Unsupported BMP format. Use a 24-bit uncompressed BMP."),
    (b"Invalid BMP image", "Invalid BMP image."),
    (b"Unable to access the file", "Unable to access the file."),
    (b"Message is too large for this image",
     "Message is too large for this image."),
    (b"Message exceeds available image capacity",
     "Message exceeds available image capacity."),
    (b"Memory allocation failed", "The program ran out of memory."),
    (b"Invalid input argument", "Invalid input given to the program."),
    (b"Saving failed", "Saving the output image failed. Check the path."),
]


def run_stegovault(input_text):
    """Run StegoVault.exe, feed it input_text, and return its raw output bytes.

    No shell is used and no console window is shown.
    """
    result = subprocess.run(
        [EXE_PATH],
        input=input_text.encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=BASE_DIR,
        timeout=30,
        check=False,
        creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
    )
    # The C program writes in Windows text mode (\n becomes \r\n); undo that.
    return result.stdout.replace(b"\r\n", b"\n")


def find_error(output):
    """Return a friendly message for a known error in the output, else None."""
    for needle, friendly in KNOWN_ERRORS:
        if needle in output:
            return friendly
    return None


def estimate_capacity(width, height):
    """Capacity estimate that matches the C code, capped at 4096 bytes."""
    return min(MAX_MESSAGE_SIZE, max(0, (width * height * 3 - HEADER_BITS) // 8))


class StegoVaultApp:
    def __init__(self, root):
        self.root = root
        root.title("StegoVault")
        root.geometry("920x700")
        root.minsize(780, 620)
        root.configure(bg=BG)

        # Tk variables bound to the input widgets
        self.mode = tk.StringVar(value="encode")
        self.image_path = tk.StringVar()
        self.output_path = tk.StringVar()
        self.message = tk.StringVar()
        self.password = tk.StringVar()
        self.show_password = tk.BooleanVar(value=False)
        self.counter_text = tk.StringVar(value="0 / 4096 bytes")
        self.info_text = tk.StringVar(value="No image selected.")
        self.status_text = tk.StringVar()

        self.preview_source = None   # PIL image currently previewed
        self.preview_photo = None    # must keep a reference or Tk drops it
        self.capacity = None         # estimated capacity of selected image

        self._setup_styles()
        self._build_ui()
        self.message.trace_add("write", self._update_counter)
        self._on_mode_change()

    # ------------------------------------------------------------------
    # Styling
    # ------------------------------------------------------------------
    def _setup_styles(self):
        style = ttk.Style()
        style.theme_use("clam")
        style.configure("TFrame", background=CARD)
        style.configure("Card.TLabel", background=CARD, foreground=TEXT,
                        font=(FONT, 10))
        style.configure("Muted.TLabel", background=CARD, foreground=MUTED,
                        font=(FONT, 9))
        style.configure("Field.TLabel", background=CARD, foreground=TEXT,
                        font=(FONT, 10, "bold"))
        style.configure("TRadiobutton", background=CARD, foreground=TEXT,
                        font=(FONT, 10))
        style.map("TRadiobutton", background=[("active", CARD)])
        style.configure("TCheckbutton", background=CARD, foreground=MUTED,
                        font=(FONT, 9))
        style.map("TCheckbutton", background=[("active", CARD)])
        style.configure("TEntry", fieldbackground="#FFFFFF", bordercolor=BORDER,
                        lightcolor=BORDER, darkcolor=BORDER, padding=6)
        style.configure("Primary.TButton", background=PRIMARY,
                        foreground="#FFFFFF", font=(FONT, 11, "bold"),
                        padding=(12, 10), borderwidth=0)
        style.map("Primary.TButton",
                  background=[("disabled", "#9DB7F3"),
                              ("active", PRIMARY_HOVER)],
                  foreground=[("disabled", "#FFFFFF")])
        style.configure("Secondary.TButton", background="#EEF2FF",
                        foreground=PRIMARY, font=(FONT, 10, "bold"),
                        padding=(10, 6), borderwidth=0)
        style.map("Secondary.TButton", background=[("active", "#DDE6FF")])

    def _card(self, parent):
        """A white 'card' frame with a thin border."""
        return tk.Frame(parent, bg=CARD, highlightthickness=1,
                        highlightbackground=BORDER, highlightcolor=BORDER)

    # ------------------------------------------------------------------
    # Layout
    # ------------------------------------------------------------------
    def _build_ui(self):
        # Header
        header = tk.Frame(self.root, bg=BG)
        header.pack(fill="x", padx=24, pady=(18, 8))
        tk.Label(header, text="STEGOVAULT", bg=BG, fg=TEXT,
                 font=(FONT, 22, "bold")).pack(anchor="w")
        tk.Label(header,
                 text="Hide a message inside an image, or recover one securely.",
                 bg=BG, fg=MUTED, font=(FONT, 10)).pack(anchor="w")

        # Status bar (packed first at the bottom so it never gets cut off)
        self.status_label = tk.Label(self.root, textvariable=self.status_text,
                                     bg=BG, fg=MUTED, font=(FONT, 10),
                                     anchor="w", justify="left",
                                     wraplength=860)
        self.status_label.pack(side="bottom", fill="x", padx=24, pady=(4, 14))

        # Body with two columns
        body = tk.Frame(self.root, bg=BG)
        body.pack(fill="both", expand=True, padx=24, pady=4)
        body.columnconfigure(0, weight=3, uniform="col")
        body.columnconfigure(1, weight=2, uniform="col")
        body.rowconfigure(0, weight=1)

        self._build_left_panel(body)
        self._build_right_panel(body)

    def _build_left_panel(self, body):
        card = self._card(body)
        card.grid(row=0, column=0, sticky="nsew", padx=(0, 10))
        inner = ttk.Frame(card)
        inner.pack(fill="both", expand=True, padx=18, pady=16)
        inner.columnconfigure(0, weight=1)
        self.left = inner

        r = 0
        # --- Operation selection
        modes = ttk.Frame(inner)
        modes.grid(row=r, column=0, sticky="w", pady=(0, 12))
        ttk.Radiobutton(modes, text="Hide a message", value="encode",
                        variable=self.mode, command=self._on_mode_change
                        ).pack(side="left", padx=(0, 18))
        ttk.Radiobutton(modes, text="Recover a message", value="decode",
                        variable=self.mode, command=self._on_mode_change
                        ).pack(side="left")

        # --- BMP image
        r += 1
        ttk.Label(inner, text="BMP image", style="Field.TLabel"
                  ).grid(row=r, column=0, sticky="w")
        r += 1
        row = ttk.Frame(inner)
        row.grid(row=r, column=0, sticky="ew", pady=(3, 0))
        row.columnconfigure(0, weight=1)
        ttk.Entry(row, textvariable=self.image_path, state="readonly"
                  ).grid(row=0, column=0, sticky="ew", padx=(0, 8))
        ttk.Button(row, text="Browse", style="Secondary.TButton",
                   command=self.browse_image).grid(row=0, column=1)
        r += 1
        ttk.Label(inner, textvariable=self.info_text, style="Muted.TLabel",
                  wraplength=420, justify="left"
                  ).grid(row=r, column=0, sticky="w", pady=(3, 10))

        # --- Output filename (encode only)
        r += 1
        self.output_frame = ttk.Frame(inner)
        self.output_frame.grid(row=r, column=0, sticky="ew", pady=(0, 10))
        self.output_frame.columnconfigure(0, weight=1)
        ttk.Label(self.output_frame, text="Save encoded image as",
                  style="Field.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Entry(self.output_frame, textvariable=self.output_path
                  ).grid(row=1, column=0, sticky="ew", padx=(0, 8), pady=(3, 0))
        ttk.Button(self.output_frame, text="Change", style="Secondary.TButton",
                   command=self.browse_output).grid(row=1, column=1,
                                                    pady=(3, 0))

        # --- Secret message (encode only)
        r += 1
        self.message_frame = ttk.Frame(inner)
        self.message_frame.grid(row=r, column=0, sticky="ew", pady=(0, 10))
        self.message_frame.columnconfigure(0, weight=1)
        ttk.Label(self.message_frame, text="Secret message",
                  style="Field.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(self.message_frame, textvariable=self.counter_text,
                  style="Muted.TLabel").grid(row=0, column=1, sticky="e")
        ttk.Entry(self.message_frame, textvariable=self.message
                  ).grid(row=1, column=0, columnspan=2, sticky="ew",
                         pady=(3, 0))
        ttk.Label(self.message_frame, text="Single line only.",
                  style="Muted.TLabel").grid(row=2, column=0, sticky="w")

        # --- Password
        r += 1
        pw = ttk.Frame(inner)
        pw.grid(row=r, column=0, sticky="ew", pady=(0, 14))
        pw.columnconfigure(0, weight=1)
        ttk.Label(pw, text="Password", style="Field.TLabel"
                  ).grid(row=0, column=0, sticky="w")
        self.password_entry = ttk.Entry(pw, textvariable=self.password,
                                        show="\u2022")
        self.password_entry.grid(row=1, column=0, sticky="ew", pady=(3, 0))
        ttk.Checkbutton(pw, text="Show password", variable=self.show_password,
                        command=self._toggle_password
                        ).grid(row=2, column=0, sticky="w", pady=(3, 0))

        # --- Action button
        r += 1
        self.action_button = ttk.Button(inner, style="Primary.TButton",
                                        command=self.on_action)
        self.action_button.grid(row=r, column=0, sticky="ew")

        # --- Recovered message (decode only)
        r += 1
        self.result_frame = ttk.Frame(inner)
        self.result_frame.grid(row=r, column=0, sticky="nsew", pady=(14, 0))
        self.result_frame.columnconfigure(0, weight=1)
        inner.rowconfigure(r, weight=1)
        head = ttk.Frame(self.result_frame)
        head.grid(row=0, column=0, sticky="ew")
        head.columnconfigure(0, weight=1)
        ttk.Label(head, text="Recovered message", style="Field.TLabel"
                  ).grid(row=0, column=0, sticky="w")
        ttk.Button(head, text="Copy", style="Secondary.TButton",
                   command=self.copy_result).grid(row=0, column=1)
        self.result_box = tk.Text(self.result_frame, height=5, wrap="word",
                                  font=(FONT, 10), fg=TEXT, bg="#FFFFFF",
                                  relief="flat", highlightthickness=1,
                                  highlightbackground=BORDER,
                                  highlightcolor=PRIMARY, padx=8, pady=6,
                                  state="disabled")
        self.result_box.grid(row=1, column=0, sticky="nsew", pady=(4, 0))
        self.result_frame.rowconfigure(1, weight=1)

    def _build_right_panel(self, body):
        card = self._card(body)
        card.grid(row=0, column=1, sticky="nsew", padx=(10, 0))
        inner = ttk.Frame(card)
        inner.pack(fill="both", expand=True, padx=18, pady=16)
        inner.rowconfigure(1, weight=1)
        inner.columnconfigure(0, weight=1)

        ttk.Label(inner, text="IMAGE PREVIEW", style="Field.TLabel"
                  ).grid(row=0, column=0, sticky="w", pady=(0, 8))

        self.canvas = tk.Canvas(inner, bg="#F0F3F9", highlightthickness=1,
                                highlightbackground=BORDER)
        self.canvas.grid(row=1, column=0, sticky="nsew")
        self.canvas.bind("<Configure>", lambda e: self._draw_preview())

        ttk.Label(inner, text="Supported format", style="Muted.TLabel"
                  ).grid(row=2, column=0, sticky="w", pady=(10, 0))
        ttk.Label(inner, text="24-bit uncompressed BMP", style="Card.TLabel"
                  ).grid(row=3, column=0, sticky="w")

    # ------------------------------------------------------------------
    # UI helpers
    # ------------------------------------------------------------------
    def set_status(self, text, kind="info"):
        """Show a message in the status bar. kind: info / success / error."""
        colour = {"info": MUTED, "success": SUCCESS, "error": ERROR}[kind]
        self.status_label.configure(fg=colour)
        self.status_text.set(text)

    def _toggle_password(self):
        self.password_entry.configure(
            show="" if self.show_password.get() else "\u2022")

    def _update_counter(self, *_):
        size = len(self.message.get().encode("utf-8"))
        self.counter_text.set(f"{size} / {MAX_MESSAGE_SIZE} bytes")

    def _on_mode_change(self):
        """Show or hide controls depending on hide / recover mode."""
        if self.mode.get() == "encode":
            self.output_frame.grid()
            self.message_frame.grid()
            self.result_frame.grid_remove()
            self.action_button.configure(text="HIDE MESSAGE IN IMAGE")
            self.set_status("Ready to hide a message.")
        else:
            self.output_frame.grid_remove()
            self.message_frame.grid_remove()
            self.result_frame.grid()
            self.action_button.configure(text="RECOVER HIDDEN MESSAGE")
            self.set_status("Ready to recover a message.")

    def _show_result(self, text):
        self.result_box.configure(state="normal")
        self.result_box.delete("1.0", "end")
        self.result_box.insert("1.0", text)
        self.result_box.configure(state="disabled")

    def copy_result(self):
        text = self.result_box.get("1.0", "end").rstrip("\n")
        if text:
            self.root.clipboard_clear()
            self.root.clipboard_append(text)
            self.set_status("Message copied to clipboard.", "success")

    # ------------------------------------------------------------------
    # Image selection and preview
    # ------------------------------------------------------------------
    def browse_image(self):
        path = filedialog.askopenfilename(
            title="Select a BMP image",
            filetypes=[("BMP images", "*.bmp"), ("All files", "*.*")])
        if path:
            self.load_image(os.path.normpath(path))

    def load_image(self, path):
        self.image_path.set(path)
        self.preview_source = None
        self.capacity = None
        try:
            with Image.open(path) as img:
                img.load()
                self.preview_source = img.convert("RGB")
            width, height = self.preview_source.size
            self.capacity = estimate_capacity(width, height)
            self.info_text.set(
                f"{width} \u00d7 {height} px\n"
                f"Estimated message capacity: {self.capacity:,} bytes "
                f"(max {MAX_MESSAGE_SIZE:,})")
            self.set_status("Image loaded.")
        except Exception:
            # Preview failed; the C program will give the final verdict.
            self.info_text.set("Preview unavailable. The file may not be a "
                               "valid BMP.")
            self.set_status("Could not preview this image. Unsupported BMP "
                            "format?", "error")
        self._draw_preview()

        # Suggest an output name next to the original: name_hidden.bmp
        folder, filename = os.path.split(path)
        stem = os.path.splitext(filename)[0]
        self.output_path.set(os.path.join(folder, stem + "_hidden.bmp"))

    def _draw_preview(self):
        """Draw the preview scaled to fit the canvas, keeping aspect ratio."""
        self.canvas.delete("all")
        cw = max(self.canvas.winfo_width(), 2)
        ch = max(self.canvas.winfo_height(), 2)
        if self.preview_source is None:
            self.canvas.create_text(cw // 2, ch // 2, text="No image selected",
                                    fill=MUTED, font=(FONT, 10))
            return
        iw, ih = self.preview_source.size
        scale = min((cw - 16) / iw, (ch - 16) / ih)
        new_size = (max(1, int(iw * scale)), max(1, int(ih * scale)))
        resized = self.preview_source.resize(new_size, Image.LANCZOS)
        self.preview_photo = ImageTk.PhotoImage(resized)
        self.canvas.create_image(cw // 2, ch // 2, image=self.preview_photo)

    def browse_output(self):
        initial = self.output_path.get()
        path = filedialog.asksaveasfilename(
            title="Save encoded image as",
            defaultextension=".bmp",
            initialdir=os.path.dirname(initial) if initial else None,
            initialfile=os.path.basename(initial) if initial else None,
            filetypes=[("BMP images", "*.bmp")])
        if path:
            self.output_path.set(os.path.normpath(path))

    # ------------------------------------------------------------------
    # Validation
    # ------------------------------------------------------------------
    @staticmethod
    def _path_problem(path):
        """The C program reads paths in the Windows ANSI code page, so keep
        to plain ASCII paths. Returns a problem description or None."""
        if not path.isascii():
            return ("The file path contains non-English characters, which "
                    "StegoVault.exe cannot open. Move the file to a simpler "
                    "path.")
        if len(path.encode("utf-8")) > MAX_PATH_BYTES:
            return "The file path is too long."
        return None

    def _check_common(self):
        """Checks shared by hide and recover. Returns an error text or None."""
        if not os.path.isfile(EXE_PATH):
            return ("StegoVault.exe was not found next to gui.py:\n"
                    + EXE_PATH)
        path = self.image_path.get()
        if not path:
            return "Please select a BMP image first."
        if not os.path.isfile(path):
            return "The selected image does not exist."
        problem = self._path_problem(path)
        if problem:
            return problem
        pw = self.password.get()
        if not pw:
            return "Please enter a password."
        if len(pw.encode("utf-8")) > MAX_PASSWORD_SIZE:
            return f"Password is too long (maximum {MAX_PASSWORD_SIZE} bytes)."
        return None

    def _check_encode(self):
        error = self._check_common()
        if error:
            return error
        msg = self.message.get()
        if not msg:
            return "Please enter a secret message."
        size = len(msg.encode("utf-8"))
        if size > MAX_MESSAGE_SIZE:
            return f"Message is too long (maximum {MAX_MESSAGE_SIZE} bytes)."
        if self.capacity is not None and size > self.capacity:
            return "Message is too large for this image."
        out = self.output_path.get().strip()
        if not out:
            return "Please choose where to save the encoded image."
        problem = self._path_problem(out)
        if problem:
            return problem
        same = (os.path.normcase(os.path.abspath(out))
                == os.path.normcase(os.path.abspath(self.image_path.get())))
        if same:
            return ("The output file must be different from the original "
                    "image.")
        if not os.path.isdir(os.path.dirname(os.path.abspath(out))):
            return "The output folder does not exist."
        return None

    # ------------------------------------------------------------------
    # Running the C program
    # ------------------------------------------------------------------
    def on_action(self):
        self.action_button.state(["disabled"])
        try:
            if self.mode.get() == "encode":
                self.do_encode()
            else:
                self.do_decode()
        except subprocess.TimeoutExpired:
            self._fail("The operation timed out.")
        except OSError as exc:
            self._fail(f"Could not launch StegoVault.exe: {exc}")
        except Exception as exc:  # last line of defence: never crash the GUI
            self._fail(f"Unexpected error: {exc}")
        finally:
            # Always re-enable the button, whatever happened above.
            self.action_button.state(["!disabled"])

    def _fail(self, text):
        self.set_status(text, "error")
        messagebox.showerror("StegoVault", text)

    def do_encode(self):
        error = self._check_encode()
        if error:
            self._fail(error)
            return

        out = self.output_path.get().strip()
        if not out.lower().endswith(".bmp"):
            out += ".bmp"
            self.output_path.set(out)
        out = os.path.abspath(out)

        if os.path.exists(out) and not messagebox.askyesno(
                "StegoVault", "The output file already exists.\n"
                              "Do you want to overwrite it?"):
            self.set_status("Cancelled.")
            return

        self.set_status("Hiding message...")
        self.root.update_idletasks()

        # Same sequence a user would type into the console.
        input_text = ("1\n" + self.image_path.get() + "\n" + out + "\n"
                      + self.message.get() + "\n" + self.password.get()
                      + "\n3\n")
        output = run_stegovault(input_text)

        # Only claim success if the C program says so (and the file exists).
        if ENCODE_OK in output and os.path.isfile(out):
            self.set_status("Message hidden successfully!", "success")
            messagebox.showinfo(
                "StegoVault",
                "Your message was hidden successfully!\n\nSaved to:\n" + out)
            return

        friendly = find_error(output)
        if friendly:
            self._fail(friendly)
        else:
            self._fail("Unexpected output from StegoVault.exe:\n"
                       + output.decode("utf-8", "replace").strip()[-300:])

    def do_decode(self):
        error = self._check_common()
        if error:
            self._fail(error)
            return

        self._show_result("")
        self.set_status("Recovering message...")
        self.root.update_idletasks()

        input_text = ("2\n" + self.image_path.get() + "\n"
                      + self.password.get() + "\n3\n")
        output = run_stegovault(input_text)

        if DECODE_OK in output:
            start = output.find(MESSAGE_LABEL, output.find(DECODE_OK))
            if start != -1:
                start += len(MESSAGE_LABEL)
                # The message is followed by a newline, then the menu again.
                end = output.rfind(MENU_MARKER)
                if end < start:
                    end = output.find(b"\n", start)
                    if end == -1:
                        end = len(output)
                text = output[start:end].rstrip(b"\n").decode("utf-8",
                                                              "replace")
                self._show_result(text)
                self.set_status("Hidden message recovered successfully!",
                                "success")
                return

        friendly = find_error(output)
        if friendly:
            self._fail(friendly)
        else:
            self._fail("Unexpected output from StegoVault.exe:\n"
                       + output.decode("utf-8", "replace").strip()[-300:])


def main():
    root = tk.Tk()
    StegoVaultApp(root)
    root.mainloop()


if __name__ == "__main__":
    main()