#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <irrKlang/irrKlang.h>
using namespace irrklang;

ISoundEngine* gSound = nullptr;
#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path, bool gammaCorrection);
void renderQuad();
void renderQuad_cross();
void renderHealthBar(float healthPercent, bool inner);
void RenderText(Shader& s, const std::string& text,
    float x, float y, float scale, glm::vec3 color);

// Text rendering
struct Character {
    unsigned int TextureID;
    glm::ivec2   Size;
    glm::ivec2   Bearing;
    unsigned int Advance;
};

std::map<char, Character> Characters;
unsigned int textVAO = 0;
unsigned int textVBO = 0;


unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrComponents;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrComponents, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
bool bloom = true;
float exposure = 1.0f;
int programChoice = 1;
float bloomFilterRadius = 0.005f;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 4.0f));
float lastX = (float)SCR_WIDTH / 2.0;
float lastY = (float)SCR_HEIGHT / 2.0;
bool firstMouse = true;
float yawMin = -120.0f;
float yawMax = 0.0f;

// bullets
struct Bullet {
    glm::vec3 position;
    glm::vec3 direction;
    float speed;
};
std::vector<Bullet> bullets;
std::vector<Bullet> enemyBullets;

float shootCooldown = 0.25f;
float timeSinceLastShot = 0.0f;

float enemyShootCooldown = 1.5f;
float timeSinceLastEnemyShot = 0.0f;


// player
glm::vec3 playerPosition = glm::vec3(0.0f, -0.8f, 3.0f);
float playerSpeed = 3.0f;
// health + score
float playerHealth = 100.0f;
const float playerMaxHealth = 100.0f;
int   playerScore = 0;

enum EnemyState { ENEMY_ALIVE, ENEMY_DYING, ENEMY_DEAD };
enum GameState { GAME_START, GAME_PLAYING, GAME_PAUSED, GAME_OVER };
GameState gameState = GAME_START;

struct Enemy {
    glm::vec3 position;
    glm::vec3 color;
    bool alive = true;
    EnemyState state = ENEMY_ALIVE;
    float deathT = 0.0f;
    float flashT = 0.0f;
};

const float kFlashDur = 0.12f;
const float kFlashBoost = 1.2f;
// Player hit flash
float playerFlashT = 0.0f;           
const float kPlayerFlashDur = 1.0f;
const float kPlayerFlashBoost = 2.0f;

std::vector<Enemy> enemies;
float enemySpeed = 1.0f;
bool moveRight = true;
float enemyStepDown = 0.05f;
float randomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// bloom stuff
struct bloomMip
{
    glm::vec2 size;
    glm::ivec2 intSize;
    unsigned int texture;
};

class bloomFBO
{
public:
    bloomFBO();
    ~bloomFBO();
    bool Init(unsigned int windowWidth, unsigned int windowHeight, unsigned int mipChainLength);
    void Destroy();
    void BindForWriting();
    const std::vector<bloomMip>& MipChain() const;

private:
    bool mInit;
    unsigned int mFBO;
    std::vector<bloomMip> mMipChain;
};

bloomFBO::bloomFBO() : mInit(false) {}
bloomFBO::~bloomFBO() {}

bool bloomFBO::Init(unsigned int windowWidth, unsigned int windowHeight, unsigned int mipChainLength)
{
    if (mInit) return true;

    glGenFramebuffers(1, &mFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

    glm::vec2 mipSize((float)windowWidth, (float)windowHeight);
    glm::ivec2 mipIntSize((int)windowWidth, (int)windowHeight);
    // Safety check
    if (windowWidth > (unsigned int)INT_MAX || windowHeight > (unsigned int)INT_MAX) {
        std::cerr << "Window size conversion overflow - cannot build bloom FBO!" << std::endl;
        return false;
    }

    for (GLuint i = 0; i < mipChainLength; i++)
    {
        bloomMip mip;

        mipSize *= 0.5f;
        mipIntSize /= 2;
        mip.size = mipSize;
        mip.intSize = mipIntSize;

        glGenTextures(1, &mip.texture);
        glBindTexture(GL_TEXTURE_2D, mip.texture);
        // we are downscaling an HDR color buffer, so we need a float texture format
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R11F_G11F_B10F,
            (int)mipSize.x, (int)mipSize.y,
            0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        std::cout << "Created bloom mip " << mipIntSize.x << 'x' << mipIntSize.y << std::endl;
        mMipChain.emplace_back(mip);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, mMipChain[0].texture, 0);

    // setup attachments
    unsigned int attachments[1] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, attachments);

    // check completion status
    int status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("gbuffer FBO error, status: 0x%x\n", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mInit = true;
    return true;
}

void bloomFBO::Destroy()
{
    for (int i = 0; i < (int)mMipChain.size(); i++) {
        glDeleteTextures(1, &mMipChain[i].texture);
        mMipChain[i].texture = 0;
    }
    glDeleteFramebuffers(1, &mFBO);
    mFBO = 0;
    mInit = false;
}

void bloomFBO::BindForWriting()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
}

const std::vector<bloomMip>& bloomFBO::MipChain() const
{
    return mMipChain;
}

class BloomRenderer
{
public:
    BloomRenderer();
    ~BloomRenderer();
    bool Init(unsigned int windowWidth, unsigned int windowHeight);
    void Destroy();
    void RenderBloomTexture(unsigned int srcTexture, float filterRadius);
    unsigned int BloomTexture();
    unsigned int BloomMip_i(int index);

private:
    void RenderDownsamples(unsigned int srcTexture);
    void RenderUpsamples(float filterRadius);

    bool mInit;
    bloomFBO mFBO;
    glm::ivec2 mSrcViewportSize;
    glm::vec2 mSrcViewportSizeFloat;
    Shader* mDownsampleShader;
    Shader* mUpsampleShader;

    bool mKarisAverageOnDownsample = true;
};

BloomRenderer::BloomRenderer() : mInit(false) {}
BloomRenderer::~BloomRenderer() {}

bool BloomRenderer::Init(unsigned int windowWidth, unsigned int windowHeight)
{
    if (mInit) return true;
    mSrcViewportSize = glm::ivec2(windowWidth, windowHeight);
    mSrcViewportSizeFloat = glm::vec2((float)windowWidth, (float)windowHeight);

    // Framebuffer
    const unsigned int num_bloom_mips = 6; // TODO: Play around with this value
    bool status = mFBO.Init(windowWidth, windowHeight, num_bloom_mips);
    if (!status) {
        std::cerr << "Failed to initialize bloom FBO - cannot create bloom renderer!\n";
        return false;
    }

    // Shaders
    mDownsampleShader = new Shader("6.new_downsample.vs", "6.new_downsample.fs");
    mUpsampleShader = new Shader("6.new_upsample.vs", "6.new_upsample.fs");

    // Downsample
    mDownsampleShader->use();
    mDownsampleShader->setInt("srcTexture", 0);
    glUseProgram(0);

    // Upsample
    mUpsampleShader->use();
    mUpsampleShader->setInt("srcTexture", 0);
    glUseProgram(0);

    return true;
}

void BloomRenderer::Destroy()
{
    mFBO.Destroy();
    delete mDownsampleShader;
    delete mUpsampleShader;
}

void BloomRenderer::RenderDownsamples(unsigned int srcTexture)
{
    const std::vector<bloomMip>& mipChain = mFBO.MipChain();

    mDownsampleShader->use();
    mDownsampleShader->setVec2("srcResolution", mSrcViewportSizeFloat);
    if (mKarisAverageOnDownsample) {
        mDownsampleShader->setInt("mipLevel", 0);
    }

    // Bind srcTexture (HDR color buffer) as initial texture input
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, srcTexture);

    // Progressively downsample through the mip chain
    for (int i = 0; i < (int)mipChain.size(); i++)
    {
        const bloomMip& mip = mipChain[i];
        glViewport(0, 0, mip.size.x, mip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, mip.texture, 0);
        // Render screen-filled quad of resolution of current mip
        renderQuad();
        // Set current mip resolution as srcResolution for next iteration
        mDownsampleShader->setVec2("srcResolution", mip.size);
        // Set current mip as texture input for next iteration
        glBindTexture(GL_TEXTURE_2D, mip.texture);
        // Disable Karis average for consequent downsamples
        if (i == 0) { mDownsampleShader->setInt("mipLevel", 1); }
    }

    glUseProgram(0);
}

void BloomRenderer::RenderUpsamples(float filterRadius)
{
    const std::vector<bloomMip>& mipChain = mFBO.MipChain();

    mUpsampleShader->use();
    mUpsampleShader->setFloat("filterRadius", filterRadius);

    // Enable additive blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    for (int i = (int)mipChain.size() - 1; i > 0; i--)
    {
        const bloomMip& mip = mipChain[i];
        const bloomMip& nextMip = mipChain[i - 1];

        // Bind viewport and texture from where to read
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mip.texture);

        // Set framebuffer render target (we write to this texture)
        glViewport(0, 0, nextMip.size.x, nextMip.size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D, nextMip.texture, 0);

        // Render screen-filled quad of resolution of current mip
        renderQuad();
    }
    glDisable(GL_BLEND);
    glUseProgram(0);
}

void BloomRenderer::RenderBloomTexture(unsigned int srcTexture, float filterRadius)
{
    mFBO.BindForWriting();

    this->RenderDownsamples(srcTexture);
    this->RenderUpsamples(filterRadius);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // Restore viewport
    glViewport(0, 0, mSrcViewportSize.x, mSrcViewportSize.y);
}

GLuint BloomRenderer::BloomTexture()
{
    return mFBO.MipChain()[0].texture;
}

GLuint BloomRenderer::BloomMip_i(int index)
{
    const std::vector<bloomMip>& mipChain = mFBO.MipChain();
    int size = (int)mipChain.size();
    return mipChain[(index > size - 1) ? size - 1 : (index < 0) ? 0 : index].texture;
}


unsigned int quadVAO = 0;
unsigned int quadVBO;
unsigned int crosshairTexture;
unsigned int crossVAO = 0;
unsigned int crossVBO = 0;
unsigned int healthVAO = 0;
unsigned int healthVBO = 0;

void drawCrosshair(float size = 20.0f, float lineWidth = 2.0f) {
    glLineWidth(lineWidth);
    glBegin(GL_LINES);

    // Horizontal line
    glVertex2f(-size, 0.0f);
    glVertex2f(size, 0.0f);

    // Vertical line
    glVertex2f(0.0f, -size);
    glVertex2f(0.0f, size);

    glEnd();
}

glm::vec3 hsv2rgb(float h, float s, float v)
{
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60.0f) { r = c; g = x; b = 0.0f; }
    else if (h < 120.0f) { r = x; g = c; b = 0.0f; }
    else if (h < 180.0f) { r = 0.0f; g = c; b = x; }
    else if (h < 240.0f) { r = 0.0f; g = x; b = c; }
    else if (h < 300.0f) { r = x; g = 0.0f; b = c; }
    else { r = c; g = 0.0f; b = x; }

    return glm::vec3(r + m, g + m, b + m);
}

glm::vec3 randomBrightColor()
{
    float h = randomFloat(0.0f, 360.0f);
    float s = 0.9f;
    float v = 1.0f;
    return hsv2rgb(h, s, v);
}

// Stage control variables
int currentStage = 1;
float stageTimer = 0.0f;
float stageDuration = 20.0f;

void respawnEnemy(Enemy& e)
{
    float x = randomFloat(-4.0f, 4.0f);
    float y = randomFloat(2.0f, 6.0f);
    float z = randomFloat(-30.0f, -15.0f);

    e.position = glm::vec3(x, y, z);
    e.state = ENEMY_ALIVE;
    e.alive = true;
    e.deathT = 0.0f;
    e.flashT = 0.0f;
    e.color = randomBrightColor();
}

void resetGame()
{
    // reset player
    playerHealth = playerMaxHealth;
    playerScore = 0;
    playerPosition = glm::vec3(0.0f, -0.8f, 3.0f);
    playerFlashT = 0.0f;

    // clear bullets
    bullets.clear();
    enemyBullets.clear();

    // respawn all enemies
    for (Enemy& e : enemies) {
        respawnEnemy(e);
    }

    timeSinceLastShot = 0.0f;
    timeSinceLastEnemyShot = 0.0f;

    // go back to start screen (or set GAME_PLAYING if you want immediate restart)
    gameState = GAME_START;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // tell GLFW to capture our mouse
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    gSound = createIrrKlangDevice();
    if (!gSound) {
        std::cerr << "Failed to create irrKlang device\n";
    }
    else {
        const std::string musicPath = FileSystem::getPath("resources/audio/bg.mp3");
        gSound->play2D(musicPath.c_str(), /*looped=*/true, /*startPaused=*/false, /*track=*/false);

    }

    glEnable(GL_DEPTH_TEST);
    Shader skyboxShader("6.sky_box.vs", "6.sky_box.fs");
    Shader shader("6.bloom.vs", "6.bloom.fs");
    Shader shaderBloomFinal("6.bloom_final.vs", "6.bloom_final.fs");
    Shader crosshairShader("crosshair.vs", "crosshair.fs");
    Shader textShader("text.vs", "text.fs");
    Model ufoModel = Model(FileSystem::getPath("resources/objects/ufo/SpaceShip.dae"));
    Model playerModel = Model(FileSystem::getPath("resources/objects/ufo/Rocket.dae"));
    Model bulletModel = Model(FileSystem::getPath("resources/objects/ufo/9mm.dae"));
    const std::string playerHitPath = FileSystem::getPath("resources/audio/damage.wav");

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    unsigned int skyboxVAO, skyboxVBO;
    float skyboxVertices[] = {
        // positions          
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    // load textures
    // -------------
    unsigned int woodTexture = loadTexture(FileSystem::getPath("resources/textures/wood.png").c_str(), true); // note that we're loading the texture as an SRGB texture
    unsigned int containerTexture = loadTexture(FileSystem::getPath("resources/textures/container2.png").c_str(), true); // note that we're loading the texture as an SRGB texture

    crosshairTexture = loadTexture(FileSystem::getPath("resources/textures/crosshair.png").c_str(), true);
    if (crosshairTexture == 0) {
        std::cerr << "Failed to load crosshair texture!" << std::endl;
    }

    std::vector<std::string> faces
    {
        FileSystem::getPath("resources/textures/nightskybox/right.png"),
        FileSystem::getPath("resources/textures/nightskybox/left.png"),
        FileSystem::getPath("resources/textures/nightskybox/top.png"),
        FileSystem::getPath("resources/textures/nightskybox/bottom.png"),
        FileSystem::getPath("resources/textures/nightskybox/front.png"),
        FileSystem::getPath("resources/textures/nightskybox/back.png"),
    };
    unsigned int cubemapTexture = loadCubemap(faces);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);
    // configure (floating point) framebuffers
    // ---------------------------------------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // attach texture to framebuffer
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // create and attach depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    // tell OpenGL which color attachments we'll use (of this framebuffer) for rendering
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer for blurring
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
        // also check if framebuffers are complete (no need for depth buffer)
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }


    // shader configuration
    // --------------------
    shader.use();
    shader.setInt("diffuseTexture", 0);
    shaderBloomFinal.use();
    shaderBloomFinal.setInt("scene", 0);
    shaderBloomFinal.setInt("bloomBlur", 1);
    crosshairShader.use();
    crosshairShader.setInt("crosshairTex", 0);

    // bloom renderer
    // --------------
    BloomRenderer bloomRenderer;
    bloomRenderer.Init(SCR_WIDTH, SCR_HEIGHT);

    // Initialize enemies with random positions instead of fixed grid
    for (int i = 0; i < 15; ++i)
    {
        Enemy e;
        respawnEnemy(e);
        enemies.push_back(e);
    }

    // ================== FreeType text init ==================
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library\n";
    }

    FT_Face face;
    if (FT_New_Face(ft,
        FileSystem::getPath("resources/fonts/OCRAEXT.ttf").c_str(),
        0,
        &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font\n";
    }

    FT_Set_Pixel_Sizes(face, 0, 48);  // 48 px font size

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // disable byte-alignment restriction

    for (unsigned char c = 0; c < 128; c++) {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cout << "ERROR::FREETYPE: Failed to load Glyph\n";
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left,  face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Text rendering VAO/VBO
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    // (x, y, u, v)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // projection matrix for text (pixel coords)
    glm::mat4 textProjection = glm::ortho(
        0.0f, static_cast<float>(SCR_WIDTH),
        0.0f, static_cast<float>(SCR_HEIGHT)
    );
    textShader.use();
    textShader.setMat4("projection", textProjection);
    textShader.setInt("text", 0);


    // render loop
    // -----------
    while (!glfwWindowShouldClose(window))
    {
        // per-frame time logic
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window);

        // Make camera follow player (stick behind)
        camera.Position = playerPosition + glm::vec3(0.0f, 0.75f, 1.0f);

        // render
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ---------- GAME LOGIC: only run when playing ----------
        if (gameState == GAME_PLAYING) {
            timeSinceLastShot += deltaTime;
            timeSinceLastEnemyShot += deltaTime;

            // ---- player bullets spawn + movement ----
            if (timeSinceLastShot >= shootCooldown)
            {
                Bullet b;
                b.position = playerPosition + glm::vec3(0.0f, 0.2f, 0.0f);
                b.direction = glm::normalize(camera.Front);
                b.speed = 10.0f;
                bullets.push_back(b);
                timeSinceLastShot = 0.0f;
            }

            for (int i = 0; i < bullets.size(); )
            {
                bullets[i].position += bullets[i].direction * bullets[i].speed * deltaTime;
                if (bullets[i].position.y > 5.0f)
                    bullets.erase(bullets.begin() + i);
                else
                    i++;
            }

            // ---- enemy bullets update + hit player ----
            for (int i = 0; i < (int)enemyBullets.size(); )
            {
                enemyBullets[i].position += enemyBullets[i].direction * enemyBullets[i].speed * deltaTime;

                if (enemyBullets[i].position.z > 10.0f || enemyBullets[i].position.z < -60.0f) {
                    enemyBullets.erase(enemyBullets.begin() + i);
                    continue;
                }

                float distToPlayer = glm::length(enemyBullets[i].position - playerPosition);
                if (distToPlayer < 0.5f) {
                    if (gSound) gSound->play2D(playerHitPath.c_str(), false);

                    playerFlashT = kPlayerFlashDur;
                    playerHealth -= 10.0f;
                    if (playerHealth < 0.0f) playerHealth = 0.0f;

                    enemyBullets.erase(enemyBullets.begin() + i);
                }
                else {
                    ++i;
                }
            }

            const std::string hitPath = FileSystem::getPath("resources/audio/hit.wav");
            // ---- player bullets hit enemies ----
            for (Bullet& b : bullets) {
                for (Enemy& e : enemies) {
                    if (!e.alive) continue;
                    float dist = glm::length(b.position - e.position);
                    if (dist < 0.5f) {
                        if (e.state == ENEMY_ALIVE) {
                            e.state = ENEMY_DYING;
                            e.deathT = 0.0f;
                            e.flashT = kFlashDur;
                            if (gSound) gSound->play2D(hitPath.c_str(), false);
                            playerScore += 5;
                        }
                        b.position.y = 9999.0f;
                    }
                }
            }

            // ---- Remove bullets that hit enemies / went too far ----
            for (int i = 0; i < bullets.size();) {
                if (bullets[i].position.y > 5.0f || bullets[i].position.y > 100.0f)
                    bullets.erase(bullets.begin() + i);
                else
                    i++;
            }

            // ---- Enemy movement ----
            float leftLimit = -5.0f;
            float rightLimit = 5.0f;
            float moveSpeed = 2.5f;
            glm::vec3 moveDir = glm::normalize(glm::vec3(0.0f, -0.3f, 1.0f));

            for (Enemy& e : enemies) {
                if (!e.alive && e.state == ENEMY_DEAD) {
                    respawnEnemy(e);
                    continue;
                }
                if (!e.alive) continue;
                e.position += moveDir * moveSpeed * deltaTime;
                e.position.x += sin(glfwGetTime() + e.position.z) * 0.002f;
                if (e.position.z > playerPosition.z + 1.0f) {
                    respawnEnemy(e);
                }
            }

            // ---- Enemy shooting ----
            if (timeSinceLastEnemyShot >= enemyShootCooldown) {
                std::vector<int> aliveIndices;
                for (int i = 0; i < (int)enemies.size(); ++i) {
                    if (enemies[i].alive && enemies[i].state == ENEMY_ALIVE) {
                        aliveIndices.push_back(i);
                    }
                }

                if (!aliveIndices.empty()) {
                    int idx = aliveIndices[rand() % aliveIndices.size()];
                    Enemy& shooter = enemies[idx];

                    Bullet b;
                    b.position = shooter.position;
                    b.direction = glm::normalize(playerPosition - shooter.position);
                    b.speed = 8.0f;
                    enemyBullets.push_back(b);
                }

                timeSinceLastEnemyShot = 0.0f;
            }

            // ---- Enemy–player collision ----
            for (Enemy& e : enemies) {
                if (!e.alive) continue;

                float distToPlayer = glm::length(e.position - playerPosition);
                if (distToPlayer < 0.7f) {
                    if (gSound) gSound->play2D(playerHitPath.c_str(), false);

                    playerFlashT = kPlayerFlashDur;
                    playerHealth -= 20.0f;
                    if (playerHealth < 0.0f) playerHealth = 0.0f;

                    respawnEnemy(e);
                }
            }

            // ---- Edge bounce ----
            bool bounce = false;
            for (const Enemy& e : enemies) {
                if (!e.alive) continue;
                if ((moveRight && e.position.x > rightLimit) ||
                    (!moveRight && e.position.x < leftLimit)) {
                    bounce = true;
                    break;
                }
            }
            if (bounce) {
                moveRight = !moveRight;
                for (Enemy& e : enemies) {
                    e.position.y -= enemyStepDown;
                }
            }

            // ---- Flash / death timers ----
            for (Enemy& e : enemies) {
                if (e.flashT > 0.0f) {
                    e.flashT = std::max(0.0f, e.flashT - deltaTime);
                }
                if (e.state == ENEMY_DYING) {
                    e.deathT += deltaTime;
                    if (e.deathT >= 0.35f) {
                        e.state = ENEMY_DEAD;
                        e.alive = false;
                    }
                }
            }

            if (playerFlashT > 0.0f) {
                playerFlashT = std::max(0.0f, playerFlashT - deltaTime);
            }
            // ---- Check for game over ----
            if (playerHealth <= 0.0f) {
                playerHealth = 0.0f;
                gameState = GAME_OVER;
            }

        } // <-- NOW this closes AFTER all movement/shooting/collisions




        // 1. render scene into floating point framebuffer
        // -----------------------------------------------
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);
        shader.use();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        // Simple camera light
        shader.setVec3("lights[0].Position", camera.Position);
        shader.setVec3("lights[0].Color", glm::vec3(100.0f, 100.0f, 100.0f)); // bright white

        shader.setVec3("viewPos", camera.Position);
        glBindTexture(GL_TEXTURE_2D, containerTexture);

        // Draw player
        glm::vec3 playerColor(0.1f, 0.4f, 0.8f);
        shader.use();
        shader.setVec3("enemyColor", playerColor);
        float playerFlash = (playerFlashT > 0.0f)
            ? (playerFlashT / kPlayerFlashDur) * kPlayerFlashBoost
            : 0.0f;
        shader.setFloat("hitFlash", playerFlash);

        glm::mat4 playerModelMatrix = glm::mat4(1.0f);
        playerModelMatrix = glm::translate(playerModelMatrix, playerPosition);
        playerModelMatrix = glm::scale(playerModelMatrix, glm::vec3(0.30f));
        playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        playerModelMatrix = glm::rotate(playerModelMatrix, glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        glBindTexture(GL_TEXTURE_2D, containerTexture);
        shader.setMat4("model", playerModelMatrix);
        playerModel.Draw(shader);


        // Draw bullets
        shader.use();
        shader.setBool("useTintOnly", true); 
        shader.setBool("hasTexture", false);
        //shader.setVec3("enemyColor", glm::vec3(1.0f));
        shader.setVec3("enemyColor", glm::vec3(0.5f, 0.5f, 0.0f));

        for (const Bullet& b : bullets) {
            glm::mat4 M = glm::mat4(1.0f);
            M = glm::translate(M, b.position);
            // Make the model face its flight direction
            glm::vec3 d = glm::normalize(b.direction);
            float yaw = std::atan2(d.x, d.z);
            float pitch = std::asin(-d.y);
            M *= glm::rotate(glm::mat4(1), glm::radians(90.0f), glm::vec3(0, 1, 0));

            // Scale to size that fits your scene
            M = glm::scale(M, glm::vec3(0.001f));   // tweak as needed
            shader.setMat4("model", M);
            // Important: don't bind containerTexture here—let Model::Draw handle textures
            bulletModel.Draw(shader);
        }
        shader.setBool("useTintOnly", false);

        shader.setBool("useTintOnly", true);
        shader.setBool("hasTexture", true);
        shader.setVec3("enemyColor", glm::vec3(1.0f, 0.13f, 0.05f));

        for (const Bullet& b : enemyBullets) {
            glm::mat4 M = glm::mat4(1.0f);
            M = glm::translate(M, b.position);

            glm::vec3 d = glm::normalize(b.direction);
            float yaw = std::atan2(d.x, d.z);
            float pitch = std::asin(-d.y);
            M *= glm::rotate(glm::mat4(1), glm::radians(270.0f), glm::vec3(0, 1, 0));

            M = glm::scale(M, glm::vec3(0.005f));
            shader.setMat4("model", M);
            bulletModel.Draw(shader);
        }
        shader.setBool("hasTexture", true);
        shader.setBool("useTintOnly", false);



        glm::vec3 aliveColor(1.0f, 0.0f, 0.0f); // normal enemy color
        glm::vec3 dyingColor(1.0f, 0.8f, 0.2f); // warm flash when dying
        for (const Enemy& e : enemies) {
            // Skip only if fully dead; allow ENEMY_DYING to draw
            if (!e.alive && e.state != ENEMY_DYING) continue;

            glm::mat4 enemyModel = glm::mat4(1.0f);
            enemyModel = glm::translate(enemyModel, e.position);

            float scale = 0.25f;   // your base scale
            float spinDeg = 0.0f;
            float flash = 0.0f;

            if (e.state == ENEMY_DYING) {
                float t = glm::clamp(e.deathT / 0.35f, 0.0f, 1.0f);
                scale = glm::mix(0.25f, 0.0f, t);   // shrink to zero
                spinDeg = 720.0f * t;                 // fast spin
                flash = (1.0f - t) * 1.4f;          // emissive flash
                enemyModel = glm::translate(enemyModel, glm::vec3(0.0f, 0.15f * (1.0f - t), 0.0f));
            }

            // your existing base orientation
            enemyModel = glm::rotate(enemyModel, glm::radians(-70.0f), glm::vec3(1, 0, 0));
            enemyModel = glm::rotate(enemyModel, glm::radians(spinDeg), glm::vec3(0, 1, 0));
            enemyModel = glm::scale(enemyModel, glm::vec3(scale));

            shader.use();
            shader.setMat4("model", enemyModel);
            // flash goes from kFlashBoost -> 0 over kFlashDur
            float flash1 = (e.flashT > 0.0f) ? (e.flashT / kFlashDur) * kFlashBoost : 0.0f;
            shader.setFloat("hitFlash", flash1);

            glm::vec3 baseColor = e.color;

            glm::vec3 finalColor = baseColor;
            if (e.state == ENEMY_DYING) {
                finalColor = glm::mix(glm::vec3(1.0f), baseColor, 0.5f);
            }

            shader.setVec3("enemyColor", finalColor);
            ufoModel.Draw(shader);
        }

        // Draw skybox after everything else has been rendered
        glDepthMask(GL_FALSE);           // Disable depth writing
        glDepthFunc(GL_LEQUAL);          // Ensure the skybox is always behind other objects

        skyboxShader.use();
        glm::mat4 skyView = glm::mat4(glm::mat3(camera.GetViewMatrix()));  // Use the camera's view matrix without translation
        skyboxShader.setMat4("view", skyView);
        skyboxShader.setMat4("projection", projection);  // Ensure the skybox has the correct perspective projection

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);  // Draw the skybox
        glBindVertexArray(0);

        glDepthFunc(GL_LESS);  // Restore the depth function
        glDepthMask(GL_TRUE);  // Enable depth writing again


        // now end scene pass
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 3. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
        // --------------------------------------------------------------------------------------------------------------------------
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderBloomFinal.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0); // trick to bind invalid texture "0", we don't care either way!


        shaderBloomFinal.setInt("programChoice", programChoice);
        shaderBloomFinal.setFloat("exposure", exposure);
        renderQuad();

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // ----- HP BAR: black border + colored fill -----
        float hpPercent = playerHealth / playerMaxHealth;
        glm::vec3 hpColor = glm::mix(glm::vec3(1.0f, 0.0f, 0.0f),   // red when low
            glm::vec3(0.0f, 1.0f, 0.0f),   // green when full
            hpPercent);

        crosshairShader.use();

        // Outer black tube (background + border)
        crosshairShader.setVec3("color", glm::vec3(0.0f, 0.0f, 0.0f));
        renderHealthBar(1.0f, false);

        // Inner colored HP fill
        crosshairShader.setVec3("color", hpColor);
        renderHealthBar(hpPercent, true);

        // ----- HP LABEL: top-left (draw ON TOP of bar) -----
        std::string hpLabel = "HP:";
        RenderText(textShader, hpLabel,
            25.0f, SCR_HEIGHT - 40.0f,   // slightly lower
            0.6f, glm::vec3(1.0f, 1.0f, 1.0f)); // white

        // ----- SCORE TEXT: top-right -----
        std::string scoreText = "Score: " + std::to_string(playerScore);
        float scoreX = SCR_WIDTH - 200.0f;
        RenderText(textShader, scoreText,
            scoreX, SCR_HEIGHT - 40.0f,
            0.6f, glm::vec3(1.0f, 1.0f, 0.0f)); // yellow

        // ----- START / PAUSE overlay text -----
        if (gameState == GAME_START) {
            std::string nameText = "KODJENG SPACESHIP";
            std::string startText = "Press ENTER to START";
            std::string controls1 = "Move: WASD | Aim: with a mouse | Pause: P";

            float cx = SCR_WIDTH * 0.5f - 220.0f;
            float cy = SCR_HEIGHT * 0.5f + 20.0f;

            RenderText(textShader, nameText,
                cx - 10.0, cy + 40.0 ,
                1.0f, glm::vec3(2.0f, 2.0f, .0f));

            RenderText(textShader, startText,
                cx, cy-20.0,
                0.8f, glm::vec3(1.0f, 1.0f, 1.0f));

            RenderText(textShader, controls1,
                cx - 140.00 , cy - 60.0f,
                0.6f, glm::vec3(0.8f, 0.8f, 0.8f));
        }
        else if (gameState == GAME_PAUSED) {
            std::string pausedText = "PAUSED";
            std::string resumeText = "Press P to RESUME";

            float cx = SCR_WIDTH * 0.5f - 120.0f;
            float cy = SCR_HEIGHT * 0.5f + 20.0f;

            RenderText(textShader, pausedText,
                cx + 40.0 , cy,
                0.9f, glm::vec3(1.0f, 1.0f, 0.0f));

            RenderText(textShader, resumeText,
                cx - 40.0f, cy - 40.0f,
                0.7f, glm::vec3(1.0f, 1.0f, 1.0f));
        }
        else if (gameState == GAME_OVER) {
            std::string overText = "GAME OVER";
            std::string restartText = "Press R to RESTART";

            float cx = SCR_WIDTH * 0.5f - 150.0f;
            float cy = SCR_HEIGHT * 0.5f + 20.0f;

            RenderText(textShader, overText,
                cx + 10.0, cy,
                1.0f, glm::vec3(1.0f, 0.0f, 0.0f)); // red

            RenderText(textShader, restartText,
                cx - 40.0f, cy - 50.0f,
                0.7f, glm::vec3(1.0f, 1.0f, 1.0f)); // white
        }



        // ----- Crosshair in center -----
        crosshairShader.use();
        crosshairShader.setVec3("color", glm::vec3(1.0f));
        renderQuad_cross();

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);




        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    bloomRenderer.Destroy();
    glfwTerminate();
    return 0;
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texture Coords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        // setup plane VAO
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}


// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);


    // Player movement (left/right)
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        playerPosition.x -= playerSpeed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        playerPosition.x += playerSpeed * deltaTime;

    // Add vertical movement (up/down)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        playerPosition.y += playerSpeed * deltaTime; // Move up
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        playerPosition.y -= playerSpeed * deltaTime; // Move down

    // Clamp player within screen limits for X and Y
    playerPosition.x = glm::clamp(playerPosition.x, -4.0f, 4.0f); // horizontal limits
    playerPosition.y = glm::clamp(playerPosition.y, -5.0f, 1.0f); // vertical limits (adjust as needed)


    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    {
        if (exposure > 0.0f)
            exposure -= 0.001f;
        else
            exposure = 0.0f;
    }
    else if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        exposure += 0.001f;
    }

    // ---------- Start / Pause controls (edge-triggered) ----------
    static bool enterLast = false;
    int enterState = glfwGetKey(window, GLFW_KEY_ENTER);
    if (enterState == GLFW_PRESS && !enterLast) {
        if (gameState == GAME_START) {
            gameState = GAME_PLAYING;          // first start
        }
        else if (gameState == GAME_PAUSED) {
            gameState = GAME_PLAYING;          // resume from pause
        }
    }
    enterLast = (enterState == GLFW_PRESS);

    static bool pLast = false;
    int pState = glfwGetKey(window, GLFW_KEY_P);
    if (pState == GLFW_PRESS && !pLast) {
        if (gameState == GAME_PLAYING) {
            gameState = GAME_PAUSED;           // pause
        }
        else if (gameState == GAME_PAUSED) {
            gameState = GAME_PLAYING;          // resume
        }
    }
    pLast = (pState == GLFW_PRESS);

    // ---------- Restart after GAME OVER ----------
    static bool rLast = false;
    int rState = glfwGetKey(window, GLFW_KEY_R);
    if (rState == GLFW_PRESS && !rLast) {
        if (gameState == GAME_OVER) {
            resetGame();
        }
    }
    rLast = (rState == GLFW_PRESS);


}
void renderQuad_cross()
{
    if (crossVAO == 0)
    {
        float size = 0.03f;

        float vertices[] = {
            -size,  0.0f,
             size,  0.0f,

             0.0f, -size,
             0.0f,  size
        };

        glGenVertexArrays(1, &crossVAO);
        glGenBuffers(1, &crossVBO);

        glBindVertexArray(crossVAO);
        glBindBuffer(GL_ARRAY_BUFFER, crossVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,                 // vec2
            GL_FLOAT,
            GL_FALSE,
            2 * sizeof(float), // stride = 2 floats
            (void*)0
        );
    }

    glBindVertexArray(crossVAO);
    glDrawArrays(GL_LINES, 0, 4);
    glBindVertexArray(0);
}


// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Update yaw/pitch manually
    camera.Yaw += xoffset;
    camera.Pitch += yoffset;

    // Clamp the pitch (vertical)
    if (camera.Pitch > 89.0f)
        camera.Pitch = 89.0f;
    if (camera.Pitch < -89.0f)
        camera.Pitch = -89.0f;

    // Clamp yaw (horizontal)
    if (camera.Yaw > yawMax)
        camera.Yaw = yawMax;
    if (camera.Yaw < yawMin)
        camera.Yaw = yawMin;

    // Now update direction using the public function
    camera.ProcessMouseMovement(0.0f, 0.0f, true); // true = constrainPitch
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// utility function for loading a 2D texture from file
// ---------------------------------------------------
unsigned int loadTexture(char const* path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum internalFormat;
        GLenum dataFormat;
        if (nrComponents == 1)
        {
            internalFormat = dataFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        }
        else if (nrComponents == 4)
        {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

void renderHealthBar(float healthPercent, bool inner)
{
    // Clamp 0–1
    if (healthPercent < 0.0f) healthPercent = 0.0f;
    if (healthPercent > 1.0f) healthPercent = 1.0f;

    if (healthVAO == 0)
    {
        glGenVertexArrays(1, &healthVAO);
        glGenBuffers(1, &healthVBO);

        glBindVertexArray(healthVAO);
        glBindBuffer(GL_ARRAY_BUFFER, healthVBO);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,
            2,                 // vec2 position
            GL_FLOAT,
            GL_FALSE,
            2 * sizeof(float),
            (void*)0
        );
    }

    // Position of health bar in NDC (TOP-LEFT)
    float left = -0.8f;   // close to left edge
    float top = 0.945f;   // a bit lower than before (was 0.90f)
    float fullWidth = 0.7f;    // width when HP = 100%
    float height = 0.09f;   // tube thickness (was 0.06f)

    float border = 0.01f;   // black border thickness

    float l = left;
    float r = left + fullWidth;
    float t = top;
    float b = top - height;

    if (inner)
    {
        // inner colored bar, slightly inset so we see a black border
        l = left + border;
        t = top - border;
        b = top - height + border;
        float innerFullWidth = fullWidth - 2.0f * border;
        r = l + innerFullWidth * healthPercent;
    }

    float vertices[] = {
        // first triangle
        l, b,
        r, b,
        r, t,
        // second triangle
        l, b,
        r, t,
        l, t
    };

    glBindVertexArray(healthVAO);
    glBindBuffer(GL_ARRAY_BUFFER, healthVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void RenderText(Shader& s, const std::string& text,
    float x, float y, float scale, glm::vec3 color)
{
    s.use();
    s.setVec3("textColor", color);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (char c : text) {
        Character ch = Characters[c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
