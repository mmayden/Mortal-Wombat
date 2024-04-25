package main

import (
	"fmt"

	"github.com/veandco/go-sdl2/sdl"
)

func main() {
	// Initialize SDL
	if err := sdl.Init(sdl.INIT_EVERYTHING); err != nil {
		fmt.Println("Failed to initialize SDL: ", err)
		return
	}
	defer sdl.Quit()

	// Create a window
	window, err := sdl.CreateWindow("Mortal Wombat", sdl.WINDOWPOS_UNDEFINED, sdl.WINDOWPOS_UNDEFINED, 800, 600, sdl.WINDOW_SHOWN)
	if err != nil {
		fmt.Println("Failed to create window: ", err)
		return
	}
	defer window.Destroy()

	// Create renderer from window
	renderer, err := sdl.CreateRenderer(window, -1, sdl.RENDERER_ACCELERATED)
	if err != nil {
		fmt.Println("Failed to create renderer: ", err)
		return
	}
	defer renderer.Destroy()

	// Load background img
	backgroundImg, err := sdl.LoadBMP("assets/backgrounds/aussie-outbackground.bmp")
	if err != nil {
		fmt.Println("Failed to load background image: ", err)
		return
	}
	defer backgroundImg.Free()

	// Create texture from background image
	backgroundTex, err := renderer.CreateTextureFromSurface(backgroundImg)
	if err != nil {
		fmt.Println("Failed to create texture from background image: ", err)
		return
	}

	// Render loop
	for {
		// Handle events
		for event := sdl.PollEvent(); event != nil; event = sdl.PollEvent() {
			switch event.(type) {
			case *sdl.QuitEvent:
				return
			}
		}

		// Clear renderer
		renderer.Clear()

		// Copy texture to renderer
		renderer.Copy(backgroundTex, nil, nil)

		// Present the renderer
		renderer.Present()
	}
}
