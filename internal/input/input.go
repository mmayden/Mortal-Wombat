package input

import (
	"github.com/mmayden/Mortal-Wombat/internal/character"
	"github.com/veandco/go-sdl2/sdl"
)

// Initializes input handling
func Init() {
	//Additional initialization code can go here
}

// Handles SDL events
func HandleEvents(character *character.Character) {
	//Poll for events
	for event := sdl.PollEvent(); event != nil; event = sdl.PollEvent() {
		switch event.(type) {

		// Quit app if user closes window
		case *sdl.QuitEvent:
			return

		// Handle keyboard events, set key presses to flags that can be used separately
		case *sdl.KeyboardEvent:
			keyboardEvent := event.(*sdl.KeyboardEvent)
			handleKeyboardEvent(keyboardEvent, character)
		}
	}
}

// Set flags based on key presses
func handleKeyboardEvent(event *sdl.KeyboardEvent, character *character.Character) {
	// Key Down
	if event.Type == sdl.KEYDOWN {
		switch event.Keysym.Sym {
		case sdl.K_LEFT:
			character.Kick = true
		case sdl.K_RIGHT:
			character.Punch = true
		case sdl.K_UP:
			character.Counter = true
		case sdl.K_DOWN:
			character.Dig = true
		case sdl.K_SPACE:
			character.Jump = true
		case sdl.K_a:
			character.Left = true
		case sdl.K_d:
			character.Right = true
		case sdl.K_w:
			character.AimUp = true
		case sdl.K_s:
			character.AimDown = true
		}
	}
	// KEY UP, zero the values, but don't when shouldn't
	if event.Type == sdl.KEYUP {
		switch event.Keysym.Sym {
		case sdl.K_LEFT:
			character.Kick = false
		case sdl.K_RIGHT:
			character.Punch = false
		case sdl.K_UP:
			character.Counter = false
		case sdl.K_DOWN:
			character.Dig = false
		case sdl.K_SPACE:
			character.Jump = false
		case sdl.K_a:
			character.Left = false
		case sdl.K_d:
			character.Right = false
		case sdl.K_w:
			character.AimUp = false
		case sdl.K_s:
			character.AimDown = false
		}
	}
}

// Updates character's position based on set flags
func UpdateCharacterPosition(character *character.Character) {
	// Update character position based on flags
	if character.Left {
		character.X -= character.Speed
	}
	if character.Right {
		character.X += character.Speed
	}
}
