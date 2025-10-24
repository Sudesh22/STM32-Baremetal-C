import pygame, sys, serial

# -----------------------------
# Serial setup
# -----------------------------
ser = serial.Serial('COM3', 9600, timeout=0.01)  # non-blocking read
print("Listening on COM3...")

# -----------------------------
# Pygame setup
# -----------------------------
pygame.init()
WIDTH, HEIGHT = 600, 400
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Brick Breaker")

# Colors
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
BLUE = (50, 150, 255)
RED = (255, 50, 50)
GREEN = (50, 255, 50)
GRAY = (200, 200, 200)

# Font
font = pygame.font.SysFont("Arial", 28, bold=True)

# Paddle
paddle_width, paddle_height = 80, 10
paddle = pygame.Rect(WIDTH // 2 - paddle_width // 2, HEIGHT - 30, paddle_width, paddle_height)

# Ball
ball_radius = 8
ball = pygame.Rect(WIDTH // 2, HEIGHT // 2, ball_radius * 2, ball_radius * 2)
ball_dx, ball_dy = 3, -3   # ball speed

# Bricks
brick_rows, brick_cols = 5, 8
brick_width, brick_height = WIDTH // brick_cols, 20

def create_bricks():
    bricks = []
    for row in range(brick_rows):
        for col in range(brick_cols):
            brick = pygame.Rect(col * brick_width, row * brick_height + 40, brick_width - 2, brick_height - 2)
            bricks.append(brick)
    return bricks

bricks = create_bricks()

# Clock
clock = pygame.time.Clock()

# Game states
game_over = False
game_won = False
score = 0

# -----------------------------
# Helper functions
# -----------------------------
def draw_text_center(text, y, color=WHITE):
    label = font.render(text, True, color)
    rect = label.get_rect(center=(WIDTH//2, y))
    screen.blit(label, rect)

def draw_score():
    score_text = font.render(f"Score: {score}", True, WHITE)
    screen.blit(score_text, (WIDTH - 150, 10))

def reset_game():
    global ball, ball_dx, ball_dy, paddle, bricks, game_over, game_won, score
    paddle.x = WIDTH // 2 - paddle_width // 2
    paddle.y = HEIGHT - 30
    ball.x = WIDTH // 2
    ball.y = HEIGHT // 2
    ball_dx, ball_dy = 3, -3
    bricks[:] = create_bricks()
    game_over = False
    game_won = False
    score = 0

# -----------------------------
# Game loop
# -----------------------------
while True:
    screen.fill(BLACK)

    # -----------------------------
    # Read serial input
    # -----------------------------
    if ser.in_waiting > 0:
        line = ser.readline().decode('utf-8').strip()
        if line == "Left":
            paddle.x -= 12
        elif line == "Right":
            paddle.x += 12
        paddle.x = max(0, min(WIDTH - paddle_width, paddle.x))

    # -----------------------------
    # Event handling
    # -----------------------------
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            sys.exit()
        if game_over or game_won:
            if event.type == pygame.MOUSEBUTTONDOWN:
                mx, my = pygame.mouse.get_pos()
                if restart_button.collidepoint(mx, my):
                    reset_game()
                if quit_button.collidepoint(mx, my):
                    pygame.quit()
                    sys.exit()

    # -----------------------------
    # Game logic
    # -----------------------------
    if not game_over and not game_won:
        # Ball movement
        ball.x += ball_dx
        ball.y += ball_dy

        # Wall collision
        if ball.left <= 0 or ball.right >= WIDTH:
            ball_dx = -ball_dx
        if ball.top <= 0:
            ball_dy = -ball_dy

        # Paddle collision
        if ball.colliderect(paddle):
            ball_dy = -abs(ball_dy)
            hit_pos = (ball.centerx - paddle.centerx) / (paddle_width // 2)
            ball_dx = int(hit_pos * 4)
            if ball_dx == 0:
                ball_dx = 2 if hit_pos > 0 else -2

        # Brick collision
        for brick in bricks[:]:
            if ball.colliderect(brick):
                bricks.remove(brick)
                ball_dy = -ball_dy
                score += 10
                break

        # Check win condition
        if len(bricks) == 0:
            game_won = True

        # Ball falls down
        if ball.bottom >= HEIGHT:
            game_over = True

        # Draw objects
        pygame.draw.rect(screen, BLUE, paddle)
        pygame.draw.ellipse(screen, WHITE, ball)
        for brick in bricks:
            pygame.draw.rect(screen, RED, brick)

        # Draw score
        draw_score()

    else:
        # Game Over / Win screen
        if game_won:
            draw_text_center("YOU WON!", HEIGHT // 2 - 50, GREEN)
        else:
            draw_text_center("GAME OVER", HEIGHT // 2 - 50, RED)

        # Restart & Quit buttons
        restart_button = pygame.Rect(WIDTH//2 - 100, HEIGHT//2, 200, 40)
        quit_button = pygame.Rect(WIDTH//2 - 100, HEIGHT//2 + 60, 200, 40)
        pygame.draw.rect(screen, GREEN, restart_button, border_radius=5)
        pygame.draw.rect(screen, GRAY, quit_button, border_radius=5)
        draw_text_center("Restart", HEIGHT//2 + 20, BLACK)
        draw_text_center("Quit", HEIGHT//2 + 80, BLACK)

        # Show final score
        draw_text_center(f"Final Score: {score}", HEIGHT // 2 - 100, WHITE)

    pygame.display.flip()
    clock.tick(60)
