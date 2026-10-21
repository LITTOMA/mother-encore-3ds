extends Reference
var scene
var observer
func instance():
    var node=scene.instance()
    node.observer_ticket=observer.register_created(node)
    return node
