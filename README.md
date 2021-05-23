# Vulkan Minimal Compute Async

This is a simple demo that demonstrates how to use Vulkan for async compute-compute operations using two queues from different queue families.
For this demo, Vulkan is used to render the Mandelbrot set on the GPU. 

![](image.png)

# Demo

The application launches a compute shader that renders the mandelbrot set, by rendering it into a storage fractalBuffer.
The storage fractalBuffer is then read from the GPU, and saved as `.bmp`. 

## Building

The project uses CMake, and all dependencies are included, so you
should use CMake to generate a "Visual Studio Solution"/makefile,
and then use that to compile the program. If you then run the program,
a file named `mandelbrot.png` should be created. This is a Mandelbrot
set that has been rendered by using Vulkan. 
