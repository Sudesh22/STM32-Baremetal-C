import pygame, sys, random, serial

# -----------------------------
# Serial setup
# -----------------------------
ser = serial.Serial('COM3', 9600, timeout=0.01)  # non-blocking
print("Listening on COM3...")

# -----------------------------
# Pygame setup
# -----------------------------
pygame.init()
WIDTH, HEIGHT = 300, 600
CELL_SIZE = 30
COLS, ROWS = WIDTH // CELL_SIZE, HEIGHT // CELL_SIZE

screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Tetris with Joystick")

# Colors
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)
GRAY = (128, 128, 128)
COLORS = [(0, 255, 255), (0, 0, 255), (255, 165, 0), (255, 255, 0),
          (0, 255, 0), (128, 0, 128), (255, 0, 0)]

# Clock
clock = pygame.time.Clock()
fall_speed = 500  # milliseconds

# Grid
grid = [[BLACK for _ in range(COLS)] for _ in range(ROWS)]

# Tetromino shapes
SHAPES = [
    [[1, 1, 1, 1]],  # I
    [[1, 1], [1, 1]],  # O
    [[0, 1, 0], [1, 1, 1]],  # T
    [[0, 1, 1], [1, 1, 0]],  # S
    [[1, 1, 0], [0, 1, 1]],  # Z
    [[1, 0, 0], [1, 1, 1]],  # J
    [[0, 0, 1], [1, 1, 1]]   # L
]

# -----------------------------
# Helper functions
# -----------------------------
def draw_grid():
    for y in range(ROWS):
        for x in range(COLS):
            pygame.draw.rect(screen, grid[y][x], 
                             (x*CELL_SIZE, y*CELL_SIZE, CELL_SIZE, CELL_SIZE))
            pygame.draw.rect(screen, GRAY, 
                             (x*CELL_SIZE, y*CELL_SIZE, CELL_SIZE, CELL_SIZE), 1)

def create_piece():
    shape = random.choice(SHAPES)
    color = random.choice(COLORS)
    x = COLS // 2 - len(shape[0]) // 2
    y = 0
    return {'shape': shape, 'color': color, 'x': x, 'y': y}

def valid_move(piece, dx, dy):
    for row_idx, row in enumerate(piece['shape']):
        for col_idx, cell in enumerate(row):
            if cell:
                x = piece['x'] + col_idx + dx
                y = piece['y'] + row_idx + dy
                if x < 0 or x >= COLS or y >= ROWS:
                    return False
                if y >= 0 and grid[y][x] != BLACK:
                    return False
    return True

def merge_piece(piece):
    for row_idx, row in enumerate(piece['shape']):
        for col_idx, cell in enumerate(row):
            if cell:
                x = piece['x'] + col_idx
                y = piece['y'] + row_idx
                if y >= 0:
                    grid[y][x] = piece['color']

def clear_lines():
    global grid
    new_grid = [row for row in grid if BLACK in row]
    lines_cleared = ROWS - len(new_grid)
    for _ in range(lines_cleared):
        new_grid.insert(0, [BLACK for _ in range(COLS)])
    grid = new_grid
    return lines_cleared

def reset_game():
    global grid, current_piece, fall_timer, score, game_over
    grid = [[BLACK for _ in range(COLS)] for _ in range(ROWS)]
    current_piece = create_piece()
    fall_timer = 0
    score = 0
    game_over = False

# -----------------------------
# Game state
# -----------------------------
current_piece = create_piece()
fall_timer = 0
score = 0
game_over = False

# -----------------------------
# Main loop
# -----------------------------
while True:
    dt = clock.tick(60)
    if not game_over:
        fall_timer += dt

    # -----------------------------
    # Serial input
    # -----------------------------
    if ser.in_waiting > 0:
        line = ser.readline().decode('utf-8').strip()
        if not game_over:
            if line == "Left" and valid_move(current_piece, -1, 0):
                current_piece['x'] -= 1
            elif line == "Right" and valid_move(current_piece, 1, 0):
                current_piece['x'] += 1
            elif line == "Button":
                # Rotate clockwise
                new_shape = list(zip(*current_piece['shape'][::-1]))
                if valid_move({'shape': new_shape, 'x': current_piece['x'], 'y': current_piece['y']}, 0, 0):
                    current_piece['shape'] = new_shape

    # -----------------------------
    # Event handling
    # -----------------------------
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            pygame.quit()
            sys.exit()
        # Restart/Quit buttons on game over
        if game_over:
            if event.type == pygame.MOUSEBUTTONDOWN:
                mx, my = pygame.mouse.get_pos()
                if restart_button.collidepoint(mx, my):
                    reset_game()
                elif quit_button.collidepoint(mx, my):
                    pygame.quit()
                    sys.exit()

    # -----------------------------
    # Piece falling
    # -----------------------------
    if not game_over:
        if fall_timer >= fall_speed:
            if valid_move(current_piece, 0, 1):
                current_piece['y'] += 1
            else:
                merge_piece(current_piece)
                lines_cleared = clear_lines()
                score += lines_cleared * 100  # +100 per line
                current_piece = create_piece()
                if not valid_move(current_piece, 0, 0):
                    game_over = True
            fall_timer = 0

    # -----------------------------
    # Drawing
    # -----------------------------
    screen.fill(BLACK)
    draw_grid()

    if not game_over:
        # Draw current piece
        for row_idx, row in enumerate(current_piece['shape']):
            for col_idx, cell in enumerate(row):
                if cell:
                    x = current_piece['x'] + col_idx
                    y = current_piece['y'] + row_idx
                    if y >= 0:
                        pygame.draw.rect(screen, current_piece['color'],
                                         (x*CELL_SIZE, y*CELL_SIZE, CELL_SIZE, CELL_SIZE))
                        pygame.draw.rect(screen, GRAY,
                                         (x*CELL_SIZE, y*CELL_SIZE, CELL_SIZE, CELL_SIZE), 1)

    # Draw score
    score_text = pygame.font.SysFont("Arial", 24).render(f"Score: {score}", True, WHITE)
    screen.blit(score_text, (10, 10))

    # -----------------------------
    # Game Over screen
    # -----------------------------
    if game_over:
        font_big = pygame.font.SysFont("Arial", 36, bold=True)
        text = font_big.render("GAME OVER", True, (255, 0, 0))
        rect = text.get_rect(center=(WIDTH//2, HEIGHT//2 - 50))
        screen.blit(text, rect)

        # Restart & Quit buttons
        restart_button = pygame.Rect(WIDTH//2 - 100, HEIGHT//2, 200, 40)
        quit_button = pygame.Rect(WIDTH//2 - 100, HEIGHT//2 + 60, 200, 40)
        pygame.draw.rect(screen, (0, 255, 0), restart_button, border_radius=5)
        pygame.draw.rect(screen, (128, 128, 128), quit_button, border_radius=5)

        font_small = pygame.font.SysFont("Arial", 28)
        text_restart = font_small.render("Restart", True, BLACK)
        text_quit = font_small.render("Quit", True, BLACK)
        screen.blit(text_restart, text_restart.get_rect(center=restart_button.center))
        screen.blit(text_quit, text_quit.get_rect(center=quit_button.center))

    pygame.display.flip()
