# Clases

## Definición

Las clases se definen con la palabra clave `class`.

```lua
class Point(x, y)
end
```

---

## Constructor `init`

El constructor se llama `init` y recibe los parámetros de la clase (sin `self`).

```lua
class Point(x, y)
    func init(x, y)
        this.x = x
        this.y = y
    end
end

p = Point(3, 4)
print(p.x)  # 3
print(p.y)  # 4
```

---

## Métodos

Los métodos se definen con `func` y NO llevan `self` como primer parámetro.

```lua
class Point(x, y)
    func init(x, y)
        this.x = x
        this.y = y
    end
    
    func show()
        print("Point(" + str(this.x) + ", " + str(this.y) + ")")
    end
    
    func distance(other)
        dx = this.x - other.x
        dy = this.y - other.y
        return sqrt(dx * dx + dy * dy)
    end
end

p1 = Point(0, 0)
p2 = Point(3, 4)

p1.show()              # Point(0, 0)
print(p1.distance(p2))  # 5
```

---

## Atributos

Los atributos se crean asignando a `this.atributo`.

```lua
class Person(nombre)
    func init(nombre)
        this.nombre = nombre
        this.edad = 0
    end
    
    func birthday()
        this.edad = this.edad + 1
    end
end

p = Person("Ava")
p.birthday()
print(p.nombre)  # Ava
print(p.edad)    # 1
```

---

## Herencia con `base()`

```lua
class Animal(nombre)
    func init(nombre)
        this.nombre = nombre
    end
    
    func speak()
        print("...")
    end
end

class Dog(nombre)
    func init(nombre)
        base(nombre)
    end
    
    func speak()
        print(this.nombre + " says Woof!")
    end
end

class Cat(nombre)
    func init(nombre)
        base(nombre)
    end
    
    func speak()
        print(this.nombre + " says Meow!")
    end
end

d = Dog("Buddy")
c = Cat("Whiskers")

d.speak()  # Buddy says Woof!
c.speak()  # Whiskers says Meow!
```

### Constructor del padre

```lua
class Vehicle(make, model, year)
    func init(make, model, year)
        this.make = make
        this.model = model
        this.year = year
    end
end

class Car(make, model, year, doors)
    func init(make, model, year, doors)
        base(make, model, year)
        this.doors = doors
    end
    
    func info()
        print(str(this.year) + " " + this.make + " " + this.model 
              + " (" + str(this.doors) + " doors)")
    end
end

car = Car("Toyota", "Corolla", 2023, 4)
car.info()  # 2023 Toyota Corolla (4 doors)
```

---

## Clases sin parámetros

```lua
class Logger()
    func init()
        this.messages = []
    end
    
    func log(msg)
        this.messages = this.messages + [msg]
    end
    
    func show()
        print(this.messages)
    end
end

logger = Logger()
logger.log("Started")
logger.log("Processing")
logger.show()  # [Started, Processing]
```

---

## Ejemplos

### Pila (Stack)

```lua
class Stack()
    func init()
        this.items = []
    end
    
    func push(item)
        this.items = this.items + [item]
    end
    
    func pop()
        if len(this.items) == 0 then
            return nil
        end
        top = this.items[len(this.items) - 1]
        this.items = this.items[0:len(this.items) - 1]
        return top
    end
    
    func empty()
        return len(this.items) == 0
    end
end

s = Stack()
s.push(1)
s.push(2)
s.push(3)
print(s.pop())  # 3
print(s.pop())  # 2
```

### Figuras geométricas

```lua
class Shape()
    func area()
        return 0
    end
    
    func perimeter()
        return 0
    end
end

class Rectangle(w, h)
    func init(w, h)
        this.w = w
        this.h = h
    end
    
    func area()
        return this.w * this.h
    end
    
    func perimeter()
        return 2 * (this.w + this.h)
    end
end

class Circle(r)
    func init(r)
        this.r = r
    end
    
    func area()
        return 3.14159 * this.r * this.r
    end
    
    func perimeter()
        return 2 * 3.14159 * this.r
    end
end

rect = Rectangle(4, 5)
circle = Circle(3)

print(rect.area())       # 20
print(rect.perimeter())  # 18
print(circle.area())     # 28.27
print(circle.perimeter()) # 18.85
```

---

## Resumen

| Característica | Sintaxis |
|----------------|----------|
| Definir | `class Name(...) ... end` |
| Constructor | `func init(params) ... end` |
| Métodos | `func nombre(params) ... end` |
| Referencia a instancia | `this.atributo` |
| Atributos | `this.nombre = valor` |
| Herencia | `base(param1, param2)` en `init` |
| Instanciar | `obj = ClassName(args)` |
| Llamar método | `obj.method(args)` |

---

## Notas

- Se usa `this` para referenciar la instancia actual (no `self`)
- El constructor se llama `init` (no `__init__`)
- Los métodos no llevan `self` como primer parámetro
- La referencia a la instancia (`this`) se inyecta automáticamente