package input

import (
	"github.com/veandco/go-sdl2/sdl"
)

// Initializes input handling
func Init() {
	//Additional initialization code can go here
}

// Handles SDL events
func HandleEvents() {
	xdir := 0
	ydir := 0
	punch := 0
	kick := 0
	counter := 0
	dig := 0
	jump := 0
	//Poll for events
	for event := sdl.PollEvent(); event != nill; event = sdl.PollEvent() {
		switch event.(type) {

		// Quit app if user closes window
		case *sdl.QuitEvent:
			return

		// Handle keyboard events, set key presses to flags that can be used separately
		case *sdl.KeyboardEvent:
			keyboardEvent := event.(*sdl.KeyboardEvent)

		// KEY DOWN
		case SDL_KEYDOWN:
			switch event.Key().Keysym.Sym {
			case sdl.K_LEFT:
				// kick
				break
			case sdl.K_RIGHT:
				// punch
				break
			case sdl.K_UP:
				// counter
				break
			case sdl.K_DOWN:
				// dig
				break
			case sdl.K_SPACE:
				// jump
				break
			case sdl.K_A:
				// left
				break
			case sdl.K_D:
				// right
				break
			case sdl.K_W:
				// up
				break
			case sdl.K_S:
				// down
				break
			}
		// KEY UP, zero the values, but don't when shouldn't
		case SDL_KEYUP:
			switch event.Key().Keysym.Sym {
			case sdl.K_LEFT:
				if xvel < 0 {
					xvel = 0
					break
				}
			case sdl.K_RIGHT:
				if xvel > 0 {
					xvel = 0
					break
				}
			case sdl.K_UP:
				if xvel < 0 {
					xvel = 0
					break
				}
			case sdl.K_DOWN:
				if xvel > 0 {
					xvel = 0
					break
				}
			case sdl.K_SPACE:
				if xvel < 0 {
					jump = 0
					break
				}
			case sdl.K_A:
				if xdir < 0 {
					xdir = 0
					break
				}
			case sdl.K_D:
				if xdir > 0 {
					xdir = 0
					break
				}
			case sdl.K_W:
				if ydir < 0 {
					ydir = 0
					break
				}
			case sdl.K_S:
				if ydir > 0 {
					ydir = 0
					break
				}
			}
		}
		break

	}
}
