# FAT16 File System Simulator

This project is a **simulation of a simplified FAT16 file system** implemented in the C programming language. It was developed to provide hands-on experience with file system concepts, low-level memory management, and custom shell command parsing.

The project includes a shell interface that allows users to interact with a virtual partition (`fat.part`) as if it were a real file system.

---

## 🧠 Overview

- **Partition Size**: 4MB
- **File System Format**: FAT16
- **Cluster Size**: 1024 bytes (2 sectors of 512 bytes)
- **Total Clusters**: 4096
- **Partition Layout**:
  - Boot Block: 1 cluster
  - FAT Table: 8 clusters (4096 entries × 2 bytes)
  - Root Directory: 1 cluster (up to 32 entries)
  - Data Area: 4086 clusters

All data (files and directories) are allocated in **cluster-sized units**, and all file operations are performed via **custom shell commands**.

---

## 🧰 Features

The shell supports the following commands:

| Command | Description |
|---------|-------------|
| `init` | Formats and initializes the virtual partition |
| `load` | Loads the FAT from the virtual disk into memory |
| `ls [/path]` | Lists the contents of a directory (default: root) |
| `mkdir /path` | Creates a new directory |
| `create /path` | Creates a new empty file |
| `unlink /path` | Deletes a file or empty directory |
| `write "content" /path` | Writes data to a file (overwrites) |
| `append "content" /path` | Appends data to the end of a file |
| `read /path` | Prints the content of a file |
| `exit` | Exits the simulator |

---

## 🔧 Technical Details

- The FAT is loaded entirely into memory (8KB).
- Only **one cluster of data** is loaded into memory at a time (no full disk load).
- Maximum of **32 entries per directory** (32B per entry, 1024B per cluster).
- File system structures are consistent with FAT16, with specific attribute values:
  - `0x0000`: Free cluster
  - `0xFFFD`: Boot block
  - `0xFFFE`: Reserved (FAT area)
  - `0xFFFF`: End of file (EOF)
  - `0x0001` to `0xFFFC`: Pointer to the next cluster

Directory entries include:
- 18 bytes: File name
- 1 byte: Attribute (0 = file, 1 = directory)
- 2 bytes: First cluster
- 4 bytes: File size

---

## 📁 Project Structure

fs16_simluator/
├── bin/ # Compiled executables
│ └── shell # Shell interface binary
├── src/ # Source code
│ ├── fat_fs.h # Constants, structs, and prototypes
│ ├── fat_fs.c # Implementation of FS operations
│ └── shell.c # Main function and shell command loop
├── docs/
│ └── relatorio.pdf # Full technical documentation (in Portuguese)
├── Makefile # Automated build system
└── README.md # Project description and usage

---

## 🚀 Compilation & Usage

### Prerequisites
- GCC or any C99-compliant compiler
- Unix-like terminal environment

### To compile:
```bash
cd fs16_simluator
make
```

### To run:
```
./bin/shell
```
## 💻 Example Session
> init
File system formatted. Run 'load' to use it.

> load
File system loaded and ready.

> mkdir /docs
> create /docs/hello.txt
> write "Hello, world!" /docs/hello.txt
> read /docs/hello.txt
Hello, world!

## 📚 Documentation
A complete technical report (relatorio.pdf) is included in the docs/ folder.

### 📘 Code Documentation with Doxygen (Optional)
This project uses Doxygen-style comments in the header file fat_fs.h to document key functions, data structures, and constants of the file system. These comments are useful in IDEs and terminal-based editors (like Vim), enabling inline tooltips, function signatures, and autocomplete hints. Additionally, you can generate structured HTML or LaTeX documentation using Doxygen by following the steps below:

Install Doxygen (if not already installed):

```bash
sudo apt install doxygen   # Ubuntu/Debian
brew install doxygen       # macOS
```

Inside the project root, generate a default configuration file:

```bash
doxygen -g
```

Edit the generated Doxyfile:

- Set INPUT = ./src
- (Optional) Set OUTPUT_DIRECTORY = ./docs/doxygen

Run Doxygen to generate the documentation:

```bash
doxygen Doxyfile
```

#### 📄 Current Status of Doxygen Output
We’ve tested Doxygen generation, and the output currently includes:

✅ Structs shown correctly under “Classes” in annotated.html
⚠️ Functions and defines are not yet fully parsed or rendered
(this will be fixed with proper Doxyfile tuning and/or inline comment adjustments)


## 📜 License
This project is released under an academic-use license. Redistribution or commercial use is not permitted without explicit permission.

### 👥 Author

- - [@fromcaio](https://github.com/fromcaio) 