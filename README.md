# Kodjeng Spaceship

A small 3D arcade-style space shooter built with modern OpenGL, where you control a rocket in space, shoot incoming enemies, dodge enemy ships and bullets, and survive as long as possible while your score and HP are displayed in an on-screen HUD.

This project is based on the LearnOpenGL framework and adds:

- Physically-based bloom rendering
- Night skybox
- Model loading with Assimp
- Health bar + score overlay with FreeType
- Sound effects and background music with irrKlang
- Simple game states: **Start**, **Playing**, **Paused**, **Game Over**

---

## Features

- **Player**
  - 3D rocket model (`Rocket.dae`)
  - Smooth WASD movement in 2D plane
  - Camera follows behind the player
  - Hit-flash effect when damaged
  - HP bar in the top-left corner with color changing from green → red

- **Enemies**
  - UFO model (`SpaceShip.dae`)
  - Random bright colors
  - Move forward with subtle sine-wave horizontal motion
  - Flash + spin + shrink animation when hit
  - Respawn at random positions after death

- **Bullets**
  - Player bullets auto-fire with cooldown
  - Enemy bullets are aimed at the player
  - Both use a 3D bullet model (`9mm.dae`)
  - Collisions:
    - Player bullets vs enemies → enemy dies, score +5
    - Enemy bullets vs player → HP −10
    - Enemy ship hitting player → HP −20

- **Rendering**
  - HDR framebuffer with bloom
  - Night skybox
  - Simple crosshair in the center of the screen

- **HUD & UI**
  - HP tube with black border and colored fill
  - Text HUD using FreeType:
    - `HP:` label (top-left)
    - `Score: <number>` (top-right)
  - Overlay text for:
    - Start screen (`Press ENTER to START`)
    - Pause screen (`PAUSED`)
    - Game over screen (`GAME OVER` / `Press R to RESTART`)

- **Audio**
  - Background space ambience
  - Sound effect when hitting an enemy (coin/score sound)
  - Sound effect when the player is hit (buzzer/error sound)

---

## Controls

- **Movement**  
  - `W` / `S` – Move up / down  
  - `A` / `D` – Move left / right  
  - Mouse – Aim (camera rotation is clamped, you can’t look backwards)

- **Game state**
  - `ENTER` – Start game from the start screen / resume when on the start overlay  
  - `P` – Pause / resume  
  - `R` – Restart after **Game Over**  
  - `ESC` – Quit

- **Visual tuning**
  - `Q` – Decrease exposure  
  - `E` – Increase exposure  

Player bullets fire automatically at a fixed cooldown while you are playing.

---

## Dependencies

This project uses:

- **OpenGL 3.3 core**
- **GLFW** – window & input  
- **GLAD** – OpenGL function loader  
- **stb_image** – image loading  
- **GLM** – math library (vectors, matrices, transforms)  
- **Assimp** – model loading (`.dae` files)  
- **FreeType** – font rendering (for the HUD text)  
- **irrKlang** – audio (BGM + SFX)  
- LearnOpenGL helper classes:
  - `Shader`
  - `Camera`
  - `Model`
  - `FileSystem`
  - 
---

## Project Structure

```text
.
├── src/
│   └── physically_based_bloom.cpp
├── include/
│   ├── glad/...
│   ├── GLFW/...
│   ├── glm/...
│   ├── stb_image.h
│   ├── irrKlang/...
│   ├── assimp/...
│   └── learnopengl/
│       ├── shader.h
│       ├── camera.h
│       ├── model.h
│       └── filesystem.h
├── resources/
│   ├── audio/
│   │   ├── bg.mp3
│   │   ├── hit.wav
│   │   └── damage.wav
│   ├── fonts/
│   │   └── OCRAEXT.ttf
│   ├── objects/
│   │   └── ufo/
│   │       ├── SpaceShip.dae
│   │       ├── Rocket.dae
│   │       └── 9mm.dae
│   └── textures/
│       └── nightskybox/
│           ├── right.png
│           ├── left.png
│           ├── top.png
│           ├── bottom.png
│           ├── front.png
│           └── back.png
├── shaders/
│   ├── 6.bloom.vs
│   ├── 6.bloom.fs
│   ├── 6.bloom_final.vs
│   ├── 6.bloom_final.fs
│   ├── 6.cubemaps.vs
│   ├── 6.cubemaps.fs
│   ├── 6.sky_box.vs
│   ├── 6.sky_box.fs
│   ├── crosshair.vs
│   ├── crosshair.fs
│   ├── text.vs
│   └── text.fs
└── README.md
```

## Art, Audio & Font Credits

- **3D Models**
  - Player rocket – Rocket0402
    Rocket model from TurboSquid:
    https://www.turbosquid.com/3d-models/rocket0402-1851445

  - Enemy ship – Low Poly Spaceship 3D Model
    Enemy spaceship model from Free3D: 
    https://free3d.com/3d-model/low-poly-spaceship-37605.html

  - Bullet – Low Poly 9mm Pistol Bullet
    Bullet model from Sketchfab:
    https://sketchfab.com/3d-models/low-poly-9mm-pistol-bullet-249ddd3567354dd2845455346f2ca721

- **Audio**
  
   - Background sound – space_line by vlad.zaichyk (Freesound, via Pixabay)
     https://pixabay.com/sound-effects/space-line-27593/

   - Hit enemy sound – Winning a coin, video game (Mixkit)
     https://mixkit.co/free-sound-effects/game/

   - Hit player sound – Wrong answer bass buzzer (Mixkit)
     https://mixkit.co/free-sound-effects/buzzer/

- **Skybox**

   - Space skybox textures – Some Space Skyboxes! Why not? by Jettelly
     https://jettelly.com/blog/some-space-skyboxes-why-not

- **Font**

   - HUD text (HP / Score / overlay messages) uses the OCRAEXT.ttf font (OCR A Extended).
     This font may be available on some systems (e.g., Windows). Please check the font’s license before redistributing it.

All external assets are used here for learning and non-commercial purposes.
