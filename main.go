package main

import (
	"fmt"

	"github.com/mmayden/Mortal-Wombat/internal/input"

	"github.com/veandco/go-sdl2/sdl"
)

func main() {
	const frameDelay = 1000 / 60
	startTime := sdl.GetTicks()

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

	//Create character instance
	character := Character{X: 100, Y: 100}
	// Initialize input handling
	input.Init()
	// Handle input events
	input.HandleEvents(&character)

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

	// Load character image
	characterImg, err := sdl.LoadBMP("assets/Frenchy.bmp")
	if err != nil {
		fmt.Println("Failed to load character image: ", err)
		return
	}
	defer characterImg.Free()

	// Render loop
	for {
		// Clear renderer
		renderer.Clear()

		// Copy texture to renderer
		renderer.Copy(backgroundTex, nil, nil)

		// Render character
		renderer.Copy(characterImg, nil, &sdl.Rect{X: character.X, character.Y: 100, W: 50, H: 50})

		// Present the renderer
		renderer.Present()
	}

	//Calculate time elapsed since start of frame
	elapsedTime := sdl.GetTicks() - startTime

	//Delay to control frame rate
	if elapsedTime < frameDelay {
		sdl.Delay(frameDelay - elapsedTime)
	}
}
