# Leitor/Visualizador de Cenas 3D - Final

O projeto implementa um visualizador de cenas 3D completo usando OpenGL.

## Características Implementadas

### Requisitos Obrigatórios
1. **Múltiplos objetos OBJ** - Suporte para carregar vários arquivos .obj
2. **Materiais e texturas** - Leitura de arquivos .mtl com propriedades Ka, Ks, Kd e texturas
3. **Iluminação Phong** - Sistema de iluminação com múltiplas fontes de luz
4. **Controle de câmera** - Navegação por teclado e mouse
5. **Seleção de objetos** - Seleção com manipulação
6. **Arquivo de configuração** - Sistema de cena baseado em arquivo de texto simples

### Funcionalidades Extras
- Sistema de trajetórias com curvas paramétricas
- Iluminação de 3 pontos (key, fill, back light)
- Visualização de trajetórias em tempo real
- Highlight de objetos selecionados
- Controles parametrizáveis de iluminação
- Seleção de objetos via ray picking

## Configuração da Cena

O arquivo `scene_config.txt` define toda a cena usando formato simples:

### Formato do Arquivo de Configuração

```text
# Comentários começam com #

[camera]
position = x, y, z
yaw = valor
pitch = valor
fov = valor

[lights]
position = x, y, z
ambient = r, g, b
diffuse = r, g, b
specular = r, g, b
intensity = valor
enabled = true/false
end = identificador

[objects]
name = Nome do Objeto
file = caminho/para/arquivo.obj
translation = x, y, z
rotation = x, y, z (em graus)
scale = valor
trajectory_points = x1,y1,z1; x2,y2,z2; x3,y3,z3
trajectory_speed = valor
end = identificador
```

### Seções Disponíveis:

- **[camera]**: Posição inicial, orientação e FOV
- **[lights]**: Múltiplas luzes (termine cada uma com `end = id`)
- **[objects]**: Lista de objetos (termine cada um com `end = id`)

### Trajetórias:
- **trajectory_points**: Pontos separados por `;` e coordenadas por `,`
- **trajectory_speed**: Velocidade de movimento ao longo da trajetória

## Controles

### Câmera
- **W/A/S/D**: Movimento horizontal
- **ESPAÇO**: Subir
- **SHIFT**: Descer
- **MOUSE**: Olhar ao redor
- **SCROLL**: Zoom (FOV)

### Seleção e Manipulação
- **TAB**: Alternar entre objetos sequencialmente
- **SETAS**: Mover objeto selecionado (frente/trás/esquerda/direita)
- **PAGE UP/DOWN**: Mover objeto para cima/baixo
- **Q/E**: Escalar objeto selecionado
- **X/Y/Z**: Rotacionar objeto selecionado (15° por tecla)

### Trajetórias
- **P**: Adicionar ponto de trajetória no objeto selecionado
- **C**: Limpar trajetória do objeto selecionado
- **G**: Ativar/desativar movimento por trajetória
- **+/-**: Aumentar/Diminuir velocidade da trajetória

### Iluminação
- **1-8**: Habilitar/desabilitar luzes individuais

### Sistema
- **ESC**: Sair do programa

## Personalização

### Adicionando Novos Objetos
1. Coloque o arquivo .obj e .mtl na pasta apropriada
2. Adicione entrada no `scene_config.txt`:
```text
name = Meu Objeto
file = caminho/objeto.obj
translation = 0.0, 0.0, 0.0
rotation = 0.0, 0.0, 0.0
scale = 1.0
end = meuobjeto
```

### Criando Trajetórias

**Método 1: Interativo**
- Selecione um objeto
- Pressione **P** para adicionar pontos onde a câmera está apontando
- Pressione **G** para ativar o movimento

**Método 2: No arquivo de configuração**
```text
trajectory_points = 0,0,0; 1,1,0; 2,0,0; 0,2,1
trajectory_speed = 1.5
```