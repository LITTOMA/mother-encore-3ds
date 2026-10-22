extends Node
func shake_camera(magnitude,duration,direction):global.events.append(["camera",magnitude,duration,[direction.x,direction.y]])
