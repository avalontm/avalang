# F-Strings en AvaLang

Los f-strings permiten interpolar expresiones dentro de cadenas de texto.

## Sintaxis

```avai
$"texto {expresion} texto"
```

## Características

- Prefijo `$` antes de la comilla de apertura
- Expresiones entre `{}` se evalúan e interpolan
- Llaves literales con `{{` y `}}`
- Escape con `\` (ej: `\"`, `\\`)

## Ejemplos

```avai
name = "Mundo"
print($"Hola {name}!")

x = 10
y = 5
print($"Suma: {x + y}")

list = [1, 2, 3]
print($"Primero: {list[0]}")

class User(name)
    func greet(self)
        return $"Hola {this.name}"
    end
end

u = User("Ana")
print($"Saludo: {u.greet()}")

print($"Llaves literales: {{simbolo}}")
```

## Implementación

- **Lexer**: Token `FSTRING` reconoce `$"..."`
- **AST**: Nodo `FStringExpr` con fragmentos (literales y expresiones)
- **Compiler**: Genera bytecode que carga literales y concatena con expresiones
- **Parser interno**: Parsea expresiones entre `{}` (soporta operaciones, indexación, atributos, llamadas a métodos)

## Limitaciones

- Solo soporta expresiones simples entre `{}`
- No soporta f-strings con múltiples líneas
- No soporta format specs (como `{x:.2f}`)