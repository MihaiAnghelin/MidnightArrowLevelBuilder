#include "gameLayer.h"
#include "opengl2Dlib.h"
#include <SFML/Audio.hpp>
#include "mapRenderer.h"
#include "mapData.h"
#include "math.h"
#include "Entity.h"
#include "input.h"
#include <fstream>
#include "imgui.h"

#define BACKGROUND_R 33
#define BACKGROUND_G 38
#define BACKGROUND_B 63

#define BACKGROUNDF_R ((float)33 / (float)0xff)
#define BACKGROUNDF_G ((float)38 / (float)0xff)
#define BACKGROUNDF_B ((float)63 / (float)0xff)

extern gl2d::internal::ShaderProgram maskShader;
extern GLint maskSamplerUniform;

gl2d::Renderer2D renderer2d;
gl2d::Renderer2D stencilRenderer2d;
gl2d::Renderer2D backgroundRenderer2d;
//sf::Music music;
MapRenderer mapRenderer;
MapData mapData;

gl2d::Texture sprites;
gl2d::Texture arrowSprite;

gl2d::FrameBuffer backGroundFBO;
gl2d::Texture backgroundTexture;

std::vector<Arrow> arrows;

unsigned char currentBlock = 2;

int mapWidth, mapHeight;
char mapName[256] = {};
char* map = nullptr;

bool collidable = true;
bool nonCollidable = true;
bool showBoxes = false;

bool initGame()
{
	renderer2d.create();
	stencilRenderer2d.create();
	backgroundRenderer2d.create();
	backgroundRenderer2d.setShaderProgram(maskShader);
	//if (music.openFromFile("ding.flac"))
	//music.play();
	ShaderProgram sp{ "blocks.vert","blocks.frag" };
	sprites.loadFromFile("sprites2.png");
	arrowSprite.loadFromFile("arrow.png");
	backgroundTexture.loadFromFile("background.jpg");
	backGroundFBO.create(40 * BLOCK_SIZE, 40 * BLOCK_SIZE);

	mapRenderer.init(sp);
	mapRenderer.sprites = sprites;
	mapRenderer.upTexture.loadFromFile("top.png");
	mapRenderer.downTexture.loadFromFile("bottom.png");
	mapRenderer.leftTexture.loadFromFile("left.png");
	mapRenderer.rightTexture.loadFromFile("right.png");

	mapData.create(40, 40, map);

	mapData.ConvertTileMapToPolyMap();

	mapWidth = mapData.w;
	mapHeight = mapData.h;

	glClearColor(BACKGROUNDF_R, BACKGROUNDF_G, BACKGROUNDF_B, 1.f);

	return true;
}

bool gameLogic(float deltaTime)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	int w = getWindowSizeX();
	int h = getWindowSizeY();

	glViewport(0, 0, w, h);
	renderer2d.updateWindowMetrics(w, h);
	/*stencilRenderer2d.updateWindowMetrics(backGroundFBO.texture.GetSize().x,
		backGroundFBO.texture.GetSize().y);*/
	backgroundRenderer2d.updateWindowMetrics(w, h);


#pragma region Camera Movement
	if (platform::isKeyHeld('Q'))
	{
		renderer2d.currentCamera.zoom -= deltaTime;
	}
	if (platform::isKeyHeld('E'))
	{
		renderer2d.currentCamera.zoom += deltaTime;
	}
	if (platform::isKeyHeld('D'))
	{
		renderer2d.currentCamera.position.x += deltaTime * 120;
	}
	if (platform::isKeyHeld('A'))
	{
		renderer2d.currentCamera.position.x -= deltaTime * 120;
	}
	if (platform::isKeyHeld('W'))
	{
		renderer2d.currentCamera.position.y -= deltaTime * 120;
	}
	if (platform::isKeyHeld('S'))
	{
		renderer2d.currentCamera.position.y += deltaTime * 120;
	}
#pragma endregion

	backgroundRenderer2d.currentCamera = renderer2d.currentCamera;

	mapData.clearColorData();

#pragma region Adding Blocks Into the World

	//todo bug: buttonWasPressed works like buttonIsPressed
	if (platform::isLMouseHeld())
	{
		glm::vec2 mousePos;
		mousePos.x = platform::getRelMousePosition().x + renderer2d.currentCamera.position.x;
		mousePos.y = platform::getRelMousePosition().y + renderer2d.currentCamera.position.y;

		mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
			glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);

		if((mousePos.x) / BLOCK_SIZE<0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
			|| (mousePos.y) / BLOCK_SIZE <0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
		{
		
		}else
		{
			mapData.get((mousePos.x) / BLOCK_SIZE, (mousePos.y) / BLOCK_SIZE).type = currentBlock;
		}

	}

	if (platform::isRMouseHeld())
	{
		glm::vec2 mousePos;
		mousePos.x = platform::getRelMousePosition().x + renderer2d.currentCamera.position.x;
		mousePos.y = platform::getRelMousePosition().y + renderer2d.currentCamera.position.y;

		mousePos = gl2d::scaleAroundPoint(mousePos, renderer2d.currentCamera.position +
			glm::vec2{ renderer2d.windowW / 2, renderer2d.windowH / 2 }, 1.f / renderer2d.currentCamera.zoom);
		if ((mousePos.x) / BLOCK_SIZE < 0 || (mousePos.x) / BLOCK_SIZE >= mapData.w
			|| (mousePos.y) / BLOCK_SIZE < 0 || (mousePos.y) / BLOCK_SIZE >= mapData.h)
		{

		}
		else
		{
			mapData.get((mousePos.x) / BLOCK_SIZE, (mousePos.y) / BLOCK_SIZE).type = Block::none;
		}

	}


#pragma endregion

#pragma region Render the blocks
	for (int x = 0; x < mapData.w; x++)
	{
		for (int y = 0; y < mapData.h; y++)
		{
			if (collidable) {
				if (isColidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor = { 1,1,1,1 };
				}
			}
			else
			{
				if (isColidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor = { 1,1,1,0.2 };
				}

			}

			if (nonCollidable)
			{
				if (!isColidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor = { 1,1,1,1 };
				}
			}
			else
			{
				if (!isColidable(mapData.get(x, y).type))
				{
					mapData.get(x, y).mainColor = { 1,1,1,0.2 };
				}
			}
		}
	}
	mapRenderer.drawFromMapData(renderer2d, mapData);
#pragma endregion

#pragma region Render Map Margins
	renderer2d.renderRectangle({ 0,0, mapData.w * BLOCK_SIZE, -10 }, Colors_Turqoise);
	renderer2d.renderRectangle({ 0,0,-10, mapData.h * BLOCK_SIZE }, Colors_Turqoise);
	renderer2d.renderRectangle({ 0,mapData.h * BLOCK_SIZE, mapData.w * BLOCK_SIZE, 10 }, Colors_Turqoise);
	renderer2d.renderRectangle({ mapData.w * BLOCK_SIZE,0,10,mapData.h * BLOCK_SIZE }, Colors_Turqoise);
#pragma endregion

	if(showBoxes)
	{
		for(int y=0;y<mapData.h;y++)
		{
			for (int x = 0; x < mapData.w; x++)
			{
				if(isColidable(mapData.get(x,y).type))
				{
					renderer2d.renderRectangle({ x*BLOCK_SIZE, y*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE }, { 0.5,0.1,0.5,0.2 });
				}
			}

		}
	
	}

	renderer2d.flush();

	return true;
}

void closeGame()
{
	//music.stop();
}


void imguiFunc(float deltaTime)
{
	ImGui::Begin("Map settings");

#pragma region Open Map
	if (ImGui::Button("Open Map"))
	{
		std::ifstream inputFile("test.level");

		inputFile >> mapWidth >> mapHeight;

		char blocks[2500];
		int it = 0;
		std::string current;

		while (std::getline(inputFile, current))
		{
			for (auto i = 0; i < current.length(); i++)
			{
				std::string aux;
				while (current[i] != ',')
				{
					aux += current[i];
					i++;
				}
				blocks[it++] = static_cast<char>(std::stoi(aux));
			}
		}
		blocks[it] = NULL;

		inputFile.close();

		mapData.cleanup();
		mapData.create(mapWidth, mapHeight, blocks);
		mapData.ConvertTileMapToPolyMap();
	}
	ImGui::NewLine();
#pragma endregion

#pragma region Empty Map
	ImGui::InputInt("New Map Width", &mapWidth);
	ImGui::InputInt("New Map Height", &mapHeight);

	if (ImGui::Button("New Map"))
	{
		mapData.cleanup();
		mapData.create(mapWidth, mapHeight, nullptr);
	}
	ImGui::NewLine();
#pragma endregion

#pragma region Save Map
	static char name[250] = {};
	ImGui::InputText("OutputFile Name", name, sizeof(name));
	if (ImGui::Button("Save Map"))
	{
		char aux[250];
		strcpy(aux, name);
		strcat(aux, ".level");

		std::ofstream outputFile(aux);
		outputFile << mapWidth << std::endl << mapHeight << std::endl;
		for (int x = 0; x < mapWidth; x++)
		{
			for (int y = 0; y < mapHeight; y++)
			{
				outputFile << static_cast<int>(mapData.get(y, x).type) << ",";
			}
			outputFile << "\n";
		}
		outputFile.close();
	}
	ImGui::NewLine();
#pragma endregion

#pragma region Block Selector


	ImGui::Checkbox("Show Collidable Blocks", &collidable);
	ImGui::Checkbox("Show Non-Collidable Blocks", &nonCollidable);
	ImGui::Checkbox("Show Boxes", &showBoxes);

	gl2d::TextureAtlas spriteAtlas(BLOCK_COUNT, 4);
	unsigned char mCount = 1;
	ImGui::BeginChild("Block Selector");

	if (collidable && nonCollidable)
	{
		while (mCount < Block::lastBlock)
		{
			ImGui::PushID(mCount);
			if (ImGui::ImageButton((void*)(intptr_t)sprites.id,
				{ 60,60 }, { spriteAtlas.get(mCount - 1, 0).x, spriteAtlas.get(mCount - 1,0).y },
				{ spriteAtlas.get(mCount - 1, 0).z, spriteAtlas.get(mCount - 1,0).w }))
			{
				currentBlock = mCount;
				llog((int)currentBlock);
			}
			ImGui::PopID();

			if (mCount % 5 != 0)
				ImGui::SameLine();
			mCount++;
		}
	}
	else
	{
		if (collidable && !nonCollidable)
		{
			unsigned char localCount = 0;
			while (mCount < Block::lastBlock)
			{
				if (isColidable(mCount))
				{
					ImGui::PushID(mCount);
					if (ImGui::ImageButton((void*)(intptr_t)sprites.id,
						{ 60,60 }, { spriteAtlas.get(mCount - 1, 0).x, spriteAtlas.get(mCount - 1,0).y },
						{ spriteAtlas.get(mCount - 1, 0).z, spriteAtlas.get(mCount - 1,0).w }))
					{
						currentBlock = mCount;
						llog((int)localCount);
					}
					ImGui::PopID();
					
					 if (localCount % 5 != 0)
						ImGui::SameLine();
					localCount++;
				}
				mCount++;
			}
		}

		if (!collidable && nonCollidable)
		{
			unsigned char localCount = 0;
			while (mCount < Block::lastBlock)
			{
				if (!isColidable(mCount))
				{
					ImGui::PushID(mCount);
					if (ImGui::ImageButton((void*)(intptr_t)sprites.id,
						{ 60,60 }, { spriteAtlas.get(mCount - 1, 0).x, spriteAtlas.get(mCount - 1,0).y },
						{ spriteAtlas.get(mCount - 1, 0).z, spriteAtlas.get(mCount - 1,0).w }))
					{
						currentBlock = mCount;
						llog((int)currentBlock);
					}
					ImGui::PopID();

					 if (localCount % 5 != 0)
						ImGui::SameLine();
					localCount++;
				}
				mCount++;
			}
		}
	}
	ImGui::EndChild();

#pragma endregion

	ImGui::End();
}
