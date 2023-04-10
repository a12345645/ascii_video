This program ascii_video is a simple tool that converts a video file into ASCII art and displays it in the terminal using ncurses.

# Dependencies
- ffmpeg
- libswscale-dev
- libncurses5-dev
- libncursesw5-dev
# Installation
To install the dependencies on Ubuntu/Debian, run the following commands:

```bash=
sudo apt-get install ffmpeg
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev libncurses-dev
```
On CentOS/Red Hat:
```
sudo dnf install ffmpeg-devel ncurses-devel
```
# Building
To build the program, run the following command in the terminal:

```bash=
gcc -o ascii_video ascii_video.c -lavcodec -lavformat -lavutil -lswscale -lswresample -lncurses -lm
```
# Usage
To use the program, run the following command in the terminal:

```bash=
./ascii_video <input_file>
```
Replace <input_file> with the path to your video file.

# Note
Please note that the program only works with ASCII characters, and may not display the video with full accuracy. The output may also depend on the font and size of your terminal.