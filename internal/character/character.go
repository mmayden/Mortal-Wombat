package character

// Character
type Character struct {
	X, Y, Speed                                                  int32 //distance position
	Left, Right, Punch, Kick, Counter, Dig, Jump, AimUp, AimDown bool
}

func NewCharacter() *Character {
	return &Character{
		X:       100,
		Y:       100,
		Left:    false,
		Right:   false,
		Speed:   0,
		AimUp:   false,
		AimDown: false,
		Jump:    false,
		Punch:   false,
		Kick:    false,
		Counter: false,
		Dig:     false,
	}
}
