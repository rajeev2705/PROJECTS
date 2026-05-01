"""
VISIO Court Calibration
Click 6 points to define court zones before game starts.
"""

import cv2
import numpy as np
from dataclasses import dataclass, field
from typing import List, Tuple, Optional


@dataclass
class CourtCalibration:
    """Stores calibrated court zone points."""
    paint_tl: Tuple[int, int] = (0, 0)      # Paint top-left
    paint_tr: Tuple[int, int] = (0, 0)      # Paint top-right
    paint_bl: Tuple[int, int] = (0, 0)      # Paint bottom-left
    paint_br: Tuple[int, int] = (0, 0)      # Paint bottom-right
    three_left: Tuple[int, int] = (0, 0)    # 3pt line left edge
    three_right: Tuple[int, int] = (0, 0)   # 3pt line right edge
    calibrated: bool = False
    frame_w: int = 0
    frame_h: int = 0

    def get_zone(self, px: int, py: int) -> str:
        """Get court zone for a point using calibrated coordinates."""
        if not self.calibrated:
            # Fallback to fixed zones
            nx, ny = px / max(self.frame_w, 1), py / max(self.frame_h, 1)
            if ny > 0.85: return "BASELINE"
            if nx < 0.18 or nx > 0.82: return "CORNER"
            if ny < 0.35: return "THREE POINT"
            if 0.33 < nx < 0.67 and ny > 0.5: return "PAINT"
            return "MID RANGE"

        # Check if point is inside paint polygon
        paint_poly = np.array([
            self.paint_tl, self.paint_tr,
            self.paint_br, self.paint_bl
        ], dtype=np.float32)
        if cv2.pointPolygonTest(paint_poly, (float(px), float(py)), False) >= 0:
            return "PAINT"

        # Check if below three point line (between the two endpoints)
        three_y = (self.three_left[1] + self.three_right[1]) / 2
        three_x_left = self.three_left[0]
        three_x_right = self.three_right[0]

        # Corner: outside three point arc horizontally
        if px < three_x_left or px > three_x_right:
            if py > three_y - 50:
                return "CORNER"

        # Below three point line = inside arc
        if py > three_y:
            return "MID RANGE"

        # Above three point line
        return "THREE POINT"


class CalibrationUI:
    """Interactive court calibration using mouse clicks."""

    STEPS = [
        ("PAINT — Top Left corner", (0, 220, 80)),
        ("PAINT — Top Right corner", (0, 220, 80)),
        ("PAINT — Bottom Left corner", (0, 220, 80)),
        ("PAINT — Bottom Right corner", (0, 220, 80)),
        ("THREE POINT LINE — Left edge", (0, 165, 255)),
        ("THREE POINT LINE — Right edge", (0, 165, 255)),
    ]

    def __init__(self, frame: np.ndarray):
        self.frame = frame.copy()
        self.h, self.w = frame.shape[:2]
        self.points: List[Tuple[int, int]] = []
        self.current_step = 0
        self.done = False
        self.hover = (0, 0)

    def _mouse_callback(self, event, x, y, flags, param):
        self.hover = (x, y)
        if event == cv2.EVENT_LBUTTONDOWN and self.current_step < len(self.STEPS):
            self.points.append((x, y))
            self.current_step += 1
            if self.current_step >= len(self.STEPS):
                self.done = True

    def _draw(self):
        display = self.frame.copy()

        # Dark overlay
        overlay = display.copy()
        cv2.rectangle(overlay, (0, 0), (self.w, 80), (0, 0, 0), -1)
        cv2.addWeighted(overlay, 0.85, display, 0.15, 0, display)

        # Instructions
        if self.current_step < len(self.STEPS):
            label, color = self.STEPS[self.current_step]
            cv2.putText(display, f"CLICK: {label}",
                        (15, 30), cv2.FONT_HERSHEY_DUPLEX, 0.7, color, 2)
            cv2.putText(display,
                        f"Step {self.current_step + 1} of {len(self.STEPS)} | S = skip calibration",
                        (15, 58), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (200, 200, 200), 1)

        # Draw crosshair at hover
        cx, cy = self.hover
        cv2.line(display, (cx - 15, cy), (cx + 15, cy), (255, 255, 255), 1)
        cv2.line(display, (cx, cy - 15), (cx, cy + 15), (255, 255, 255), 1)

        # Draw placed points
        colors = [(0, 220, 80)] * 4 + [(0, 165, 255)] * 2
        labels = ["P-TL", "P-TR", "P-BL", "P-BR", "3L", "3R"]
        for i, (px, py) in enumerate(self.points):
            c = colors[i]
            cv2.circle(display, (px, py), 6, c, -1)
            cv2.putText(display, labels[i], (px + 8, py - 8),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, c, 1)

        # Draw paint polygon if 4 points placed
        if len(self.points) >= 4:
            poly = np.array([
                self.points[0], self.points[1],
                self.points[3], self.points[2]
            ])
            cv2.polylines(display, [poly], True, (0, 220, 80), 2)

        # Draw three point line if all 6 placed
        if len(self.points) >= 6:
            cv2.line(display, self.points[4], self.points[5], (0, 165, 255), 2)

        return display

    def run(self, window_name: str = "VISIO — COURT CALIBRATION") -> Optional[CourtCalibration]:
        cv2.namedWindow(window_name)
        cv2.setMouseCallback(window_name, self._mouse_callback)

        while True:
            display = self._draw()
            cv2.imshow(window_name, display)
            key = cv2.waitKey(16) & 0xFF

            if key == ord('s'):
                # Skip calibration
                cv2.destroyWindow(window_name)
                cal = CourtCalibration(calibrated=False,
                                       frame_w=self.w, frame_h=self.h)
                return cal

            if key == ord('q'):
                cv2.destroyWindow(window_name)
                return None

            if self.done:
                cv2.destroyWindow(window_name)
                cal = CourtCalibration(
                    paint_tl=self.points[0],
                    paint_tr=self.points[1],
                    paint_bl=self.points[2],
                    paint_br=self.points[3],
                    three_left=self.points[4],
                    three_right=self.points[5],
                    calibrated=True,
                    frame_w=self.w,
                    frame_h=self.h,
                )
                return cal


def run_calibration(frame: np.ndarray) -> CourtCalibration:
    """Run calibration UI and return calibration object."""
    ui = CalibrationUI(frame)
    result = ui.run()
    if result is None:
        # User quit — return uncalibrated
        return CourtCalibration(calibrated=False,
                                frame_w=frame.shape[1],
                                frame_h=frame.shape[0])
    return result
