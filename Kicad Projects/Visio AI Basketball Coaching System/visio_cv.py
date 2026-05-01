"""
VISIO — Computer Vision Module
AI-Powered Basketball Performance System
Patent Pending | Application No. 64/020,415
Copyright 2026 Abraham Sutton. All rights reserved.
"""

import asyncio
import cv2
import json
import math
import os
import queue
import subprocess
import threading
import time
from collections import deque
from dataclasses import dataclass
from datetime import datetime
from typing import List, Optional, Tuple

import numpy as np
from ultralytics import YOLO
from court_calibration import CourtCalibration, run_calibration

# ─────────────────────────────────────────────
#  BLE SERVER
#  Broadcasts live stats to the companion app
#  Service UUID: 12345678-1234-5678-1234-56789abcdef0
#  Stats Characteristic: 12345678-1234-5678-1234-56789abcdef1  (notify)
#  Command Characteristic: 12345678-1234-5678-1234-56789abcdef2 (write)
# ─────────────────────────────────────────────
VISIO_SERVICE_UUID    = "12345678-1234-5678-1234-56789abcdef0"
VISIO_STATS_UUID      = "12345678-1234-5678-1234-56789abcdef1"
VISIO_COMMAND_UUID    = "12345678-1234-5678-1234-56789abcdef2"

try:
    from bless import BlessServer, BlessGATTCharacteristic, GATTCharacteristicProperties, GATTAttributePermissions
    BLE_AVAILABLE = True
except ImportError:
    BLE_AVAILABLE = False


class VisioGATTServer:
    """
    BLE GATT server that advertises as 'VISIO' and pushes stats to the app.
    Runs in a background thread with its own asyncio event loop.
    Falls back silently if bless is not installed.
    """

    def __init__(self):
        self._server = None
        self._loop = None
        self._thread = None
        self._stats_payload = b"{}"
        self._running = False
        self.connected = False

    def start(self):
        if not BLE_AVAILABLE:
            print("[VISIO BLE] bless not installed — BLE disabled. Run: pip3 install bless")
            return
        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        try:
            self._loop.run_until_complete(self._serve())
        except Exception as e:
            print(f"[VISIO BLE] Server error: {e}")

    async def _serve(self):
        trigger = asyncio.Event()
        self._server = BlessServer(name="VISIO", loop=self._loop)
        self._server.read_request_func = self._read_request
        self._server.write_request_func = self._write_request

        await self._server.add_new_service(VISIO_SERVICE_UUID)

        # Stats characteristic — notify
        await self._server.add_new_characteristic(
            VISIO_SERVICE_UUID,
            VISIO_STATS_UUID,
            GATTCharacteristicProperties.read | GATTCharacteristicProperties.notify,
            None,
            GATTAttributePermissions.readable
        )

        # Command characteristic — write (app sends commands to goggles)
        await self._server.add_new_characteristic(
            VISIO_SERVICE_UUID,
            VISIO_COMMAND_UUID,
            GATTCharacteristicProperties.write,
            None,
            GATTAttributePermissions.writeable
        )

        await self._server.start()
        print("[VISIO BLE] Advertising as 'VISIO' — waiting for app connection...")
        await trigger.wait()

    def _read_request(self, characteristic: "BlessGATTCharacteristic", **kwargs):
        return self._stats_payload

    def _write_request(self, characteristic: "BlessGATTCharacteristic", value, **kwargs):
        try:
            cmd = json.loads(value.decode())
            print(f"[VISIO BLE] Command received: {cmd}")
        except Exception:
            pass

    def push_stats(self, pts: int, ast: int, reb: int, shot_clock: float, game_live: bool):
        """Call this every frame to update the stats broadcast."""
        if not BLE_AVAILABLE or self._server is None:
            return
        payload = json.dumps({
            "pts": pts,
            "ast": ast,
            "reb": reb,
            "sc": round(shot_clock, 1),
            "live": game_live,
            "ts": round(time.time(), 2)
        }).encode()
        self._stats_payload = payload
        # Notify connected clients
        try:
            if self._loop and self._loop.is_running():
                asyncio.run_coroutine_threadsafe(
                    self._notify(payload), self._loop
                )
        except Exception:
            pass

    async def _notify(self, payload: bytes):
        try:
            await self._server.update_value(VISIO_SERVICE_UUID, VISIO_STATS_UUID)
        except Exception:
            pass

    def stop(self):
        self._running = False


# ─────────────────────────────────────────────
#  COACHING CUES  (15 languages)
# ─────────────────────────────────────────────
CUES = {
    "en": {
        "kick_out": "KICK OUT",
        "move_ball": "MOVE THE BALL",
        "arc_low": "ARC TOO LOW",
        "arc_high": "ARC TOO HIGH",
        "defense_gap": "DEFENSE GAP — DRIVE",
        "fast_break": "PUSH — FAST BREAK",
        "double_team": "DOUBLE TEAM — KICK OUT",
        "swing": "SWING THE BALL",
        "shoot_paint": "SHOOT — PAINT OPEN",
        "post_move": "POST MOVE",
        "shoot_mid": "SHOOT — OPEN MID",
        "shoot_corner": "SHOOT — CORNER THREE",
        "drive_left": "DRIVE LEFT",
        "drive_right": "DRIVE RIGHT",
        "perfect_arc": "PERFECT ARC",
        "dead_ball": "DEAD BALL",
        "ball_live": "BALL LIVE",
        "out_of_bounds": "OUT OF BOUNDS",
        "long_pass": "LEAD THE PLAYER",
        "you_scored": "NICE BUCKET",
        "you_assisted": "GREAT PASS",
        "you_rebounded": "YOUR REBOUND",
    },
    "he": {
        "kick_out": "פתח",
        "move_ball": "הזז את הכדור",
        "arc_low": "קשת נמוכה מדי",
        "arc_high": "קשת גבוהה מדי",
        "defense_gap": "פרצה — תנהל",
        "fast_break": "מהלך מהיר",
        "double_team": "כפל סימון — פתח",
        "swing": "הסט את הכדור",
        "shoot_paint": "ירה — צבע פתוח",
        "post_move": "תנועת פוסט",
        "shoot_mid": "ירה — אמצע פתוח",
        "shoot_corner": "ירה — שלוש מפינה",
        "drive_left": "חדור שמאל",
        "drive_right": "חדור ימין",
        "perfect_arc": "קשת מושלמת",
        "dead_ball": "כדור מת",
        "ball_live": "כדור חי",
        "out_of_bounds": "מחוץ לגבולות",
        "long_pass": "הוביל את השחקן",
        "you_scored": "כדור נכנס",
        "you_assisted": "מסירה מעולה",
        "you_rebounded": "ריבאונד שלך",
    },
    "es": {
        "kick_out": "PASE AFUERA",
        "move_ball": "MUEVE EL BALÓN",
        "arc_low": "ARCO MUY BAJO",
        "arc_high": "ARCO MUY ALTO",
        "defense_gap": "BRECHA — ATACA",
        "fast_break": "ATAQUE RÁPIDO",
        "double_team": "DOBLE MARCA — PASA",
        "swing": "MUEVE EL BALÓN",
        "shoot_paint": "LANZA — ZONA LIBRE",
        "post_move": "JUGADA DE POST",
        "shoot_mid": "LANZA — MEDIA DISTANCIA",
        "shoot_corner": "LANZA — TRIPLE ESQUINA",
        "drive_left": "ATACA IZQUIERDA",
        "drive_right": "ATACA DERECHA",
        "perfect_arc": "ARCO PERFECTO",
        "dead_ball": "BALÓN MUERTO",
        "ball_live": "BALÓN VIVO",
        "out_of_bounds": "FUERA",
        "long_pass": "LIDERA AL JUGADOR",
        "you_scored": "CANASTA",
        "you_assisted": "GRAN PASE",
        "you_rebounded": "TU REBOTE",
    },
    "fr": {
        "kick_out": "PASSE",
        "move_ball": "BOUGE LE BALLON",
        "arc_low": "ARC TROP BAS",
        "arc_high": "ARC TROP HAUT",
        "defense_gap": "BRÈCHE — ATTAQUE",
        "fast_break": "CONTRE-ATTAQUE",
        "double_team": "DOUBLE — PASSE",
        "swing": "DÉPLACE LE BALLON",
        "shoot_paint": "TIRE — ZONE LIBRE",
        "post_move": "JEU DE POSTE",
        "shoot_mid": "TIRE — MI-DISTANCE",
        "shoot_corner": "TIRE — TROIS POINTS",
        "drive_left": "PÉNÈTRE GAUCHE",
        "drive_right": "PÉNÈTRE DROITE",
        "perfect_arc": "ARC PARFAIT",
        "dead_ball": "BALLON MORT",
        "ball_live": "BALLON EN JEU",
        "out_of_bounds": "HORS LIMITES",
        "long_pass": "DEVANCE LE JOUEUR",
        "you_scored": "PANIER",
        "you_assisted": "BELLE PASSE",
        "you_rebounded": "TON REBOND",
    },
    "pt": {
        "kick_out": "PASSE",
        "move_ball": "MOVE A BOLA",
        "arc_low": "TRAJETÓRIA BAIXA",
        "arc_high": "TRAJETÓRIA ALTA",
        "defense_gap": "BRECHA — ATACA",
        "fast_break": "CONTRA-ATAQUE",
        "double_team": "MARCAÇÃO DUPLA",
        "swing": "MOVE A BOLA",
        "shoot_paint": "ARREMESSA — GARRAFÃO",
        "post_move": "JOGADA DE POST",
        "shoot_mid": "ARREMESSA — MEIA",
        "shoot_corner": "ARREMESSA — TRÊS",
        "drive_left": "PENETRA ESQUERDA",
        "drive_right": "PENETRA DIREITA",
        "perfect_arc": "TRAJETÓRIA PERFEITA",
        "dead_ball": "BOLA MORTA",
        "ball_live": "BOLA VIVA",
        "out_of_bounds": "FORA",
        "long_pass": "ANTECIPE O JOGADOR",
        "you_scored": "CESTA",
        "you_assisted": "ÓTIMO PASSE",
        "you_rebounded": "SEU REBOTE",
    },
    "ar": {
        "kick_out": "تمرير",
        "move_ball": "حرك الكرة",
        "arc_low": "القوس منخفض",
        "arc_high": "القوس مرتفع",
        "defense_gap": "ثغرة — تقدم",
        "fast_break": "هجمة مرتدة",
        "double_team": "مراقبة مزدوجة",
        "swing": "قلّب الكرة",
        "shoot_paint": "ارمِ — المنطقة",
        "post_move": "لعبة البوست",
        "shoot_mid": "ارمِ — المنتصف",
        "shoot_corner": "ارمِ — الزاوية",
        "drive_left": "تقدم يسار",
        "drive_right": "تقدم يمين",
        "perfect_arc": "قوس مثالي",
        "dead_ball": "كرة ميتة",
        "ball_live": "كرة حية",
        "out_of_bounds": "خارج",
        "long_pass": "قُد اللاعب",
        "you_scored": "سلة",
        "you_assisted": "تمريرة رائعة",
        "you_rebounded": "ريباوند",
    },
    "zh": {
        "kick_out": "传球",
        "move_ball": "移动球",
        "arc_low": "弧度太低",
        "arc_high": "弧度太高",
        "defense_gap": "防守缺口 — 突破",
        "fast_break": "快攻",
        "double_team": "夹击 — 传球",
        "swing": "转移球",
        "shoot_paint": "投篮 — 禁区",
        "post_move": "低位单打",
        "shoot_mid": "投篮 — 中距离",
        "shoot_corner": "投篮 — 角落三分",
        "drive_left": "突破左侧",
        "drive_right": "突破右侧",
        "perfect_arc": "完美弧度",
        "dead_ball": "死球",
        "ball_live": "球活了",
        "out_of_bounds": "出界",
        "long_pass": "引导球员",
        "you_scored": "进球",
        "you_assisted": "精彩助攻",
        "you_rebounded": "你的篮板",
    },
    "ja": {
        "kick_out": "パス",
        "move_ball": "ボールを動かせ",
        "arc_low": "アークが低い",
        "arc_high": "アークが高い",
        "defense_gap": "守備の穴 — 突破",
        "fast_break": "速攻",
        "double_team": "ダブルチーム — パス",
        "swing": "ボールを動かせ",
        "shoot_paint": "シュート — ペイント",
        "post_move": "ポストプレー",
        "shoot_mid": "シュート — ミドル",
        "shoot_corner": "シュート — コーナー3",
        "drive_left": "ドライブ左",
        "drive_right": "ドライブ右",
        "perfect_arc": "完璧なアーク",
        "dead_ball": "デッドボール",
        "ball_live": "ボールイン",
        "out_of_bounds": "アウト",
        "long_pass": "先読みしろ",
        "you_scored": "ナイスシュート",
        "you_assisted": "ナイスパス",
        "you_rebounded": "リバウンド",
    },
    "ko": {
        "kick_out": "패스",
        "move_ball": "공을 움직여",
        "arc_low": "아크 너무 낮음",
        "arc_high": "아크 너무 높음",
        "defense_gap": "수비 틈 — 돌파",
        "fast_break": "속공",
        "double_team": "더블팀 — 패스",
        "swing": "공을 스윙해",
        "shoot_paint": "슛 — 페인트",
        "post_move": "포스트 무브",
        "shoot_mid": "슛 — 미드레인지",
        "shoot_corner": "슛 — 코너 3점",
        "drive_left": "드라이브 왼쪽",
        "drive_right": "드라이브 오른쪽",
        "perfect_arc": "완벽한 아크",
        "dead_ball": "데드볼",
        "ball_live": "볼 인플레이",
        "out_of_bounds": "아웃",
        "long_pass": "선수를 리드해",
        "you_scored": "나이스 슛",
        "you_assisted": "좋은 패스",
        "you_rebounded": "리바운드",
    },
    "de": {
        "kick_out": "PASS",
        "move_ball": "BALL BEWEGEN",
        "arc_low": "BOGEN ZU NIEDRIG",
        "arc_high": "BOGEN ZU HOCH",
        "defense_gap": "LÜCKE — ANGRIFF",
        "fast_break": "FAST BREAK",
        "double_team": "DOPPELDECKUNG",
        "swing": "BALL SCHWINGEN",
        "shoot_paint": "WERFEN — ZONE",
        "post_move": "POST-MOVE",
        "shoot_mid": "WERFEN — MITTE",
        "shoot_corner": "WERFEN — ECKE",
        "drive_left": "ANGRIFF LINKS",
        "drive_right": "ANGRIFF RECHTS",
        "perfect_arc": "PERFEKTER BOGEN",
        "dead_ball": "TOTER BALL",
        "ball_live": "BALL IM SPIEL",
        "out_of_bounds": "AUS",
        "long_pass": "SPIELER ANTIZIPIEREN",
        "you_scored": "KORB",
        "you_assisted": "TOLLER PASS",
        "you_rebounded": "DEIN REBOUND",
    },
    "it": {
        "kick_out": "PASSA",
        "move_ball": "MUOVI IL PALLONE",
        "arc_low": "ARCO BASSO",
        "arc_high": "ARCO ALTO",
        "defense_gap": "SPAZIO — ATTACCA",
        "fast_break": "CONTROPIEDE",
        "double_team": "RADDOPPIO — PASSA",
        "swing": "SPOSTA IL PALLONE",
        "shoot_paint": "TIRA — AREA",
        "post_move": "GIOCO IN POST",
        "shoot_mid": "TIRA — MEDIA",
        "shoot_corner": "TIRA — ANGOLO",
        "drive_left": "PENETRA SINISTRA",
        "drive_right": "PENETRA DESTRA",
        "perfect_arc": "ARCO PERFETTO",
        "dead_ball": "PALLA MORTA",
        "ball_live": "PALLA IN GIOCO",
        "out_of_bounds": "FUORI",
        "long_pass": "ANTICIPA IL GIOCATORE",
        "you_scored": "CANESTRO",
        "you_assisted": "BELL'ASSIST",
        "you_rebounded": "TUO RIMBALZO",
    },
    "tr": {
        "kick_out": "PAS",
        "move_ball": "TOPU HAREKET ETTİR",
        "arc_low": "YAY ALÇAK",
        "arc_high": "YAY YÜKSEK",
        "defense_gap": "BOŞLUK — ATAKLA",
        "fast_break": "HIZLI HÜCUM",
        "double_team": "ÇİFT ADAM",
        "swing": "TOPU KAYDIR",
        "shoot_paint": "ŞUTU AT — BOYA",
        "post_move": "POST HAMLESI",
        "shoot_mid": "ŞUTU AT — ORTA",
        "shoot_corner": "ŞUTU AT — KÖŞE",
        "drive_left": "SOL BOŞLUK",
        "drive_right": "SAĞ BOŞLUK",
        "perfect_arc": "MÜKEMMEL YAY",
        "dead_ball": "ÖLÜ TOP",
        "ball_live": "TOP OYUNDA",
        "out_of_bounds": "DIŞARI",
        "long_pass": "OYUNCUYU ÖNGÖR",
        "you_scored": "SAYAÇ",
        "you_assisted": "GÜZEL PAS",
        "you_rebounded": "RİBAUND",
    },
    "ru": {
        "kick_out": "ПАС",
        "move_ball": "ДВИГАЙ МЯЧ",
        "arc_low": "ДУГА НИЗКАЯ",
        "arc_high": "ДУГА ВЫСОКАЯ",
        "defense_gap": "БРЕШЬ — АТАКУЙ",
        "fast_break": "БЫСТРЫЙ ПРОРЫВ",
        "double_team": "ДВОЙНАЯ ОПЕКА",
        "swing": "ПЕРЕВЕДИ МЯЧ",
        "shoot_paint": "БРОСАЙ — ЗОНА",
        "post_move": "ИГРА В ПОСТЕ",
        "shoot_mid": "БРОСАЙ — СРЕДНЯЯ",
        "shoot_corner": "БРОСАЙ — УГОЛ",
        "drive_left": "ПРОХОД ЛЕВЫЙ",
        "drive_right": "ПРОХОД ПРАВЫЙ",
        "perfect_arc": "ИДЕАЛЬНАЯ ДУГА",
        "dead_ball": "МЯЧ МЁРТВЫЙ",
        "ball_live": "МЯЧ В ИГРЕ",
        "out_of_bounds": "ЗА ЛИНИЕЙ",
        "long_pass": "ОПЕРЕЖАЙ ИГРОКА",
        "you_scored": "БРОСОК",
        "you_assisted": "ОТЛИЧНЫЙ ПАС",
        "you_rebounded": "ПОДБОР",
    },
    "el": {
        "kick_out": "ΠΑΣ",
        "move_ball": "ΚΙΝΗΣΕ ΤΗ ΜΠΑΛΑ",
        "arc_low": "ΤΡΟΧΙΑ ΧΑΜΗΛΗ",
        "arc_high": "ΤΡΟΧΙΑ ΥΨΗΛΗ",
        "defense_gap": "ΚΕΝΟ — ΕΠΙΤΕΣΟΥ",
        "fast_break": "ΑΝΤΕΠΙΘΕΣΗ",
        "double_team": "ΔΙΠΛΗ ΜΑΡΚΑ",
        "swing": "ΜΕΤΑΚΙΝΗΣΕ",
        "shoot_paint": "ΡΙΞΕ — ΖΩΝΗ",
        "post_move": "ΠΑΙΧΝΙΔΙ ΠΟΣΤ",
        "shoot_mid": "ΡΙΞΕ — ΜΕΣΗ",
        "shoot_corner": "ΡΙΞΕ — ΓΩΝΙΑ",
        "drive_left": "ΕΙΣΧΩΡΗΣΕ ΑΡΙΣΤΕΡΑ",
        "drive_right": "ΕΙΣΧΩΡΗΣΕ ΔΕΞΙΑ",
        "perfect_arc": "ΤΕΛΕΙΑ ΤΡΟΧΙΑ",
        "dead_ball": "ΝΕΚΡΗ ΜΠΑΛΑ",
        "ball_live": "ΜΠΑΛΑ ΣΕ ΠΑΙΧΝΙΔΙ",
        "out_of_bounds": "ΕΚΤΟΣ",
        "long_pass": "ΟΔΗΓΗΣΕ ΤΟΝ ΠΑΙΚΤΗ",
        "you_scored": "ΚΑΛΑΘΙ",
        "you_assisted": "ΩΡΑΙΑ ΠΑΣΑ",
        "you_rebounded": "ΡΙΜΠΑΟΥΝΤ",
    },
    "nl": {
        "kick_out": "PAS",
        "move_ball": "BEWEEG DE BAL",
        "arc_low": "BOOG TE LAAG",
        "arc_high": "BOOG TE HOOG",
        "defense_gap": "GAT — AANVAL",
        "fast_break": "SNEL AANVAL",
        "double_team": "DUBBELE DEKKING",
        "swing": "VERPLAATS DE BAL",
        "shoot_paint": "SCHIET — ZONE",
        "post_move": "POST SPEL",
        "shoot_mid": "SCHIET — MIDDEN",
        "shoot_corner": "SCHIET — HOEK",
        "drive_left": "AANVAL LINKS",
        "drive_right": "AANVAL RECHTS",
        "perfect_arc": "PERFECTE BOOG",
        "dead_ball": "DODE BAL",
        "ball_live": "BAL IN SPEL",
        "out_of_bounds": "UIT",
        "long_pass": "ANTICIPEER DE SPELER",
        "you_scored": "BASKET",
        "you_assisted": "MOOIE PAS",
        "you_rebounded": "JOUW REBOUND",
    },
}


@dataclass
class VisioConfig:
    camera_index: int = 2
    camera_width: int = 1920
    camera_height: int = 1080
    camera_fps: int = 240
    camera_fov: int = 110
    process_width: int = 640
    process_height: int = 360
    detection_interval: int = 2
    hud_alpha: float = 0.75
    color_primary: tuple = (0, 165, 255)
    color_white: tuple = (255, 255, 255)
    color_ok: tuple = (0, 220, 80)
    color_alert: tuple = (0, 60, 255)
    color_dim: tuple = (160, 160, 160)
    ball_model: str = "basketball.pt"
    ball_conf: float = 0.35
    dribble_min_down_vy: float = 10.0
    dribble_min_up_vy: float = 10.0
    dribble_cooldown: float = 0.4
    dribble_floor_zone: float = 0.55
    shot_min_up_vy: float = 12.0
    shot_min_frames: int = 6
    shot_min_speed: float = 8.0
    arc_ideal_min: float = 45.0
    arc_ideal_max: float = 60.0
    arc_min_rsquared: float = 0.65
    arc_trail_points: int = 20
    player_confidence: float = 0.50
    player_nms: float = 0.45
    player_model: str = "basketball.pt"
    play_buffer_sec: int = 15
    play_pre_roll: int = 5
    play_post_roll: int = 3
    play_folder: str = "visio_saved_plays"
    voice_min_gap: float = 15.0
    language: str = "en"
    headless: bool = False
    streetball_mode: bool = False
    dead_ball_still_frames: int = 45
    dead_ball_still_threshold: float = 4.0
    dead_ball_missing_frames: int = 90
    dead_ball_oob_margin: float = 0.04
    shot_clock_seconds: int = 24
    ble_enabled: bool = True


class PlayerStats:
    def __init__(self):
        self.points = 0
        self.assists = 0
        self.rebounds = 0
        self.shots_taken = 0
        self.dribble_count = 0
        self.shot_count = 0
        self._shot_in_air = False
        self._shot_peak_y = None
        self._ball_coming_down = False
        self._rebound_window = False
        self._rebound_window_start = 0.0

    def update(self, ball, fh, fw, shot_active, dribble_detected, velocity):
        now = time.time()
        bx, by, br = ball if ball else (fw // 2, fh // 2, 0)

        if dribble_detected:
            self.dribble_count += 1

        if shot_active and not self._shot_in_air:
            self._shot_in_air = True
            self._shot_peak_y = by
            self.shot_count += 1
            self.shots_taken += 1

        if ball and self._shot_in_air:
            vy = velocity[1]
            if vy < 0:
                self._shot_peak_y = by
            elif vy > 5:
                self._ball_coming_down = True
            if self._ball_coming_down and by > fh * 0.6:
                hoop_x_min = fw * 0.3
                hoop_x_max = fw * 0.7
                hoop_y_max = fh * 0.45
                if (self._shot_peak_y is not None and
                        self._shot_peak_y < hoop_y_max and
                        hoop_x_min < bx < hoop_x_max):
                    self.points += 2
                    self._shot_in_air = False
                    self._ball_coming_down = False
                    return "scored"

        if self._shot_in_air and ball is None:
            self._shot_in_air = False
            self._ball_coming_down = False

        if self._rebound_window:
            if ball and now - self._rebound_window_start < 2.0:
                self.rebounds += 1
                self._rebound_window = False
                return "rebound"
            elif now - self._rebound_window_start > 2.0:
                self._rebound_window = False

        return None

    def open_rebound_window(self):
        self._rebound_window = True
        self._rebound_window_start = time.time()


class ShotClock:
    def __init__(self, seconds: int = 24):
        self.total = seconds
        self.remaining = float(seconds)
        self._last_tick = time.time()
        self._running = False

    def start(self):
        self._running = True
        self._last_tick = time.time()

    def stop(self):
        self._running = False

    def reset(self):
        self.remaining = float(self.total)
        self._last_tick = time.time()

    def tick(self) -> bool:
        if not self._running:
            return False
        now = time.time()
        self.remaining -= (now - self._last_tick)
        self._last_tick = now
        if self.remaining <= 0:
            self.remaining = 0
            self._running = False
            return True
        return False

    @property
    def display(self) -> str:
        return f"{int(self.remaining)}"


class GameState:
    LIVE = "LIVE"
    DEAD = "DEAD"
    OOB  = "OOB"

    def __init__(self, config: VisioConfig):
        self.config = config
        self.state = self.LIVE
        self.still_count = 0
        self.missing_count = 0
        self.last_ball_pos = None
        self.dead_reason = ""
        self.long_pass_active = False
        self.long_pass_start = None
        self.long_pass_timeout = 1.2
        self._state_change_time = time.time()
        self._min_dead_duration = 1.5

    def update(self, ball, fw, fh) -> str:
        cfg = self.config
        now = time.time()

        if ball is None:
            self.missing_count += 1
            self.still_count = 0
            if self.missing_count >= cfg.dead_ball_missing_frames:
                self._set_dead("FOUL / TIMEOUT")
            return self.state

        self.missing_count = 0
        bx, by, br = ball

        margin_x = int(fw * cfg.dead_ball_oob_margin)
        margin_y = int(fh * cfg.dead_ball_oob_margin)
        if (bx < margin_x or bx > fw - margin_x or
                by < margin_y or by > fh - margin_y):
            self._set_dead("OUT OF BOUNDS")
            return self.OOB

        if self.last_ball_pos is not None:
            dx = bx - self.last_ball_pos[0]
            dy = by - self.last_ball_pos[1]
            moved = math.hypot(dx, dy)
            if moved < cfg.dead_ball_still_threshold:
                self.still_count += 1
            else:
                self.still_count = 0
            if (moved > 35 and by < fh * 0.55 and abs(dx) > abs(dy) * 1.5):
                if not self.long_pass_active:
                    self.long_pass_active = True
                    self.long_pass_start = now
            else:
                if self.long_pass_active:
                    self.long_pass_active = False
                    self.long_pass_start = None

        self.last_ball_pos = (bx, by)
        dead_threshold = cfg.dead_ball_still_frames
        if cfg.streetball_mode:
            dead_threshold = int(dead_threshold * 2.5)

        if self.still_count >= dead_threshold:
            self._set_dead("STOPPED")
        elif self.state == self.DEAD:
            if now - self._state_change_time > self._min_dead_duration:
                self._set_live()

        return self.state

    def _set_dead(self, reason: str):
        if self.state != self.DEAD:
            self.state = self.DEAD
            self.dead_reason = reason
            self._state_change_time = time.time()

    def _set_live(self):
        if self.state != self.LIVE:
            self.state = self.LIVE
            self.dead_reason = ""
            self.still_count = 0
            self._state_change_time = time.time()

    def is_live(self) -> bool:
        return self.state == self.LIVE

    def is_long_pass(self) -> bool:
        if not self.long_pass_active or self.long_pass_start is None:
            return False
        return (time.time() - self.long_pass_start) < self.long_pass_timeout


class CameraManager:
    def __init__(self, config: VisioConfig):
        self.config = config
        self.cap = None
        self.frame_queue: queue.Queue = queue.Queue(maxsize=4)
        self.running = False
        self.frame_count = 0
        self.last_frame = None

    def connect(self) -> bool:
        if self.config.camera_index == -1:
            pipeline = (
                "nvarguscamerasrc ! "
                "video/x-raw(memory:NVMM),width=1920,height=1080,framerate=60/1 ! "
                "nvvidconv ! video/x-raw,format=BGRx ! "
                "videoconvert ! video/x-raw,format=BGR ! appsink"
            )
            self.cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)
        else:
            self.cap = cv2.VideoCapture(self.config.camera_index)
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.config.camera_width)
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.config.camera_height)
            self.cap.set(cv2.CAP_PROP_FPS, self.config.camera_fps)
        if not self.cap.isOpened():
            print("[VISIO CAM] ERROR: Camera not found.")
            return False
        print("[VISIO CAM] Connected.")
        return True

    def start(self):
        self.running = True
        threading.Thread(target=self._capture_loop, daemon=True).start()

    def _capture_loop(self):
        while self.running:
            ret, frame = self.cap.read()
            if not ret:
                time.sleep(0.005)
                continue
            if self.frame_queue.full():
                try:
                    self.frame_queue.get_nowait()
                except queue.Empty:
                    pass
            self.frame_queue.put(frame)
            self.last_frame = frame
            self.frame_count += 1

    def get_frame(self) -> np.ndarray:
        try:
            return self.frame_queue.get(timeout=0.05)
        except queue.Empty:
            if self.last_frame is not None:
                return self.last_frame.copy()
            blank = np.zeros(
                (self.config.camera_height, self.config.camera_width, 3), dtype=np.uint8
            )
            cv2.putText(blank, "NO CAMERA SIGNAL", (50, blank.shape[0] // 2),
                        cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0, 165, 255), 3)
            return blank

    def stop(self):
        self.running = False
        if self.cap:
            self.cap.release()


class BallTracker:
    def __init__(self, config: VisioConfig):
        self.config = config
        self.model = YOLO(config.ball_model)
        self.trajectory: deque = deque(maxlen=60)
        self.velocity: Tuple[float, float] = (0.0, 0.0)
        self.prev_velocity: Tuple[float, float] = (0.0, 0.0)
        self.last_pos: Optional[Tuple[int, int]] = None
        self.last_dribble_time = 0.0
        self.shot_up_frames = 0
        self.shot_active = False
        self._lower = np.array((5, 100, 100))
        self._upper = np.array((25, 255, 255))
        self._kernel = np.ones((5, 5), np.uint8)
        print("[VISIO BALL] YOLO ball tracker ready.")

    def detect(self, frame: np.ndarray) -> Optional[Tuple[int, int, int]]:
        fh, fw = frame.shape[:2]
        small = cv2.resize(frame, (self.config.process_width, self.config.process_height))
        sx = fw / self.config.process_width
        sy = fh / self.config.process_height
        results = self.model.predict(small, conf=self.config.ball_conf, iou=0.4, verbose=False)
        best_box = None
        best_conf = 0.0
        for r in results:
            for box in r.boxes:
                conf = float(box.conf[0])
                x1, y1, x2, y2 = box.xyxy[0].tolist()
                w = x2 - x1
                h = y2 - y1
                aspect = w / h if h > 0 else 0
                if 0.5 < aspect < 2.0 and conf > best_conf:
                    best_conf = conf
                    best_box = (x1, y1, x2, y2)
        if best_box is not None:
            x1, y1, x2, y2 = best_box
            cx = int(((x1 + x2) / 2) * sx)
            cy = int(((y1 + y2) / 2) * sy)
            r = int(max((x2 - x1), (y2 - y1)) / 2 * sx)
            return self._update(cx, cy, max(r, 8), fh)
        return self._hsv_detect(frame)

    def _hsv_detect(self, frame: np.ndarray) -> Optional[Tuple[int, int, int]]:
        hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, self._lower, self._upper)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, self._kernel)
        mask = cv2.morphologyEx(mask, cv2.MORPH_DILATE, self._kernel)
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        best, best_score = None, 0.0
        for c in contours:
            area = cv2.contourArea(c)
            if area < 300:
                continue
            perim = cv2.arcLength(c, True)
            if perim == 0:
                continue
            circ = 4 * math.pi * area / (perim * perim)
            if circ > best_score:
                best_score, best = circ, c
        if best is None or best_score < 0.70:
            return None
        (cx, cy), r = cv2.minEnclosingCircle(best)
        return self._update(int(cx), int(cy), max(int(r), 8), frame.shape[0])

    def _update(self, cx: int, cy: int, r: int, fh: int) -> Tuple[int, int, int]:
        self.prev_velocity = self.velocity
        if self.last_pos:
            self.velocity = (cx - self.last_pos[0], cy - self.last_pos[1])
            self._update_shot(fh)
        self.last_pos = (cx, cy)
        self.trajectory.append((cx, cy, time.time()))
        return (cx, cy, r)

    def _update_shot(self, fh: int):
        vy = self.velocity[1]
        speed = math.hypot(*self.velocity)
        if vy < -self.config.shot_min_up_vy and speed > self.config.shot_min_speed:
            self.shot_up_frames += 1
            self.shot_active = self.shot_up_frames >= self.config.shot_min_frames
        else:
            self.shot_up_frames = 0
            self.shot_active = False

    def is_dribble(self, fh: int) -> bool:
        now = time.time()
        if now - self.last_dribble_time < self.config.dribble_cooldown:
            return False
        if not self.last_pos:
            return False
        if self.last_pos[1] / fh < self.config.dribble_floor_zone:
            return False
        if (self.prev_velocity[1] > self.config.dribble_min_down_vy and
                self.velocity[1] < -self.config.dribble_min_up_vy):
            self.last_dribble_time = now
            return True
        return False

    def get_shot_arc(self) -> Optional[float]:
        if not self.shot_active or len(self.trajectory) < 8:
            return None
        pts = np.array(
            [(x, y) for x, y, _ in list(self.trajectory)[-self.config.arc_trail_points:]]
        )
        if len(pts) < 5:
            return None
        x = pts[:, 0].astype(float)
        y = pts[:, 1].astype(float)
        coeffs = np.polyfit(x, y, 2)
        y_pred = np.polyval(coeffs, x)
        ss_res = np.sum((y - y_pred) ** 2)
        ss_tot = np.sum((y - np.mean(y)) ** 2)
        if ss_tot == 0 or (1 - ss_res / ss_tot) < self.config.arc_min_rsquared:
            return None
        vx, vy, _, _ = cv2.fitLine(pts.astype(np.float32), cv2.DIST_L2, 0, 0.01, 0.01)
        return abs(90 - abs(math.degrees(math.atan2(float(vy), float(vx)))))


class PlayerDetector:
    def __init__(self, config: VisioConfig):
        self.config = config
        self.model = YOLO(config.player_model)
        print("[VISIO DETECT] Model loaded.")

    def detect(self, frame: np.ndarray) -> List[Tuple[int, int, int, int]]:
        small = cv2.resize(frame, (self.config.process_width, self.config.process_height))
        results = self.model.predict(
            small, classes=[0],
            conf=self.config.player_confidence,
            iou=self.config.player_nms,
            verbose=False
        )
        sx = frame.shape[1] / self.config.process_width
        sy = frame.shape[0] / self.config.process_height
        boxes = []
        for r in results:
            for box in r.boxes:
                x1, y1, x2, y2 = box.xyxy[0].tolist()
                boxes.append((
                    int(x1 * sx), int(y1 * sy),
                    int((x2 - x1) * sx), int((y2 - y1) * sy)
                ))
        return boxes

    def classify_teams(self, frame, boxes):
        if len(boxes) < 2:
            return ['A'] * len(boxes)
        samples = []
        for (x, y, w, h) in boxes:
            roi = frame[y + h // 4: y + 3 * h // 4, x: x + w]
            if roi.size == 0:
                samples.append([0.0, 0.0, 0.0])
                continue
            samples.append(cv2.cvtColor(roi, cv2.COLOR_BGR2HSV).mean(axis=(0, 1)).tolist())
        arr = np.array(samples, dtype=np.float32)
        _, labels, _ = cv2.kmeans(
            arr, 2, None,
            (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 10, 1.0),
            3, cv2.KMEANS_RANDOM_CENTERS
        )
        return ['A' if l == 0 else 'B' for l in labels.flatten()]


class PlaySaver:
    def __init__(self, config: VisioConfig):
        self.config = config
        self.buffer: deque = deque()
        self.lock = threading.Lock()
        self.max_frames = config.camera_fps * config.play_buffer_sec
        self.post_target = config.camera_fps * config.play_post_roll
        self.collecting = False
        self.post_frames: list = []
        self.post_count = 0
        self.pre_roll: list = []
        self.reason = ""
        self.save_queue: queue.Queue = queue.Queue()
        self.saved_count = 0
        self.last_save = 0.0
        os.makedirs(config.play_folder, exist_ok=True)
        threading.Thread(target=self._save_loop, daemon=True).start()

    def push(self, frame: np.ndarray):
        with self.lock:
            self.buffer.append((frame.copy(), time.time()))
            while len(self.buffer) > self.max_frames:
                self.buffer.popleft()
        if self.collecting:
            self.post_frames.append((frame.copy(), time.time()))
            self.post_count += 1
            if self.post_count >= self.post_target:
                self._finalize()

    def trigger(self, reason: str = "PLAY"):
        now = time.time()
        if now - self.last_save < 10 or self.collecting:
            return
        self.last_save = now
        self.saved_count += 1
        with self.lock:
            self.pre_roll = list(self.buffer)
        self.reason = reason
        self.post_frames = []
        self.post_count = 0
        self.collecting = True

    def manual_save(self):
        self.trigger("MANUAL")

    def _finalize(self):
        self.collecting = False
        self.save_queue.put({
            "frames": self.pre_roll + self.post_frames,
            "reason": self.reason,
            "index": self.saved_count,
            "time": datetime.now().strftime("%Y%m%d_%H%M%S"),
        })

    def _save_loop(self):
        while True:
            try:
                job = self.save_queue.get(timeout=1)
            except queue.Empty:
                continue
            frames = job["frames"]
            if not frames:
                continue
            h, w = frames[0][0].shape[:2]
            fname = f"play_{job['index']:04d}_{job['reason']}_{job['time']}.mp4"
            path = os.path.join(self.config.play_folder, fname)
            fps = min(self.config.camera_fps, 60)
            writer = cv2.VideoWriter(path, cv2.VideoWriter_fourcc(*"mp4v"), fps, (w, h))
            for frame, _ in frames:
                writer.write(frame)
            writer.release()
            print(f"[VISIO PLAYS] Saved: {path}")


class VoiceCoach:
    MAC_VOICES = {
        "en": "Evan", "he": "Carmit", "es": "Monica", "fr": "Thomas",
        "pt": "Luciana", "ar": "Maged", "zh": "Ting-Ting", "ja": "Kyoko",
        "ko": "Yuna", "de": "Anna", "it": "Alice", "tr": "Yelda",
        "ru": "Milena", "el": "Melina", "nl": "Xander",
    }

    def __init__(self, config: VisioConfig):
        self.config = config
        self.last_text = ""
        self.last_time = 0.0
        self.speaking = False
        import platform
        self._is_mac = platform.system() == "Darwin"

    def say(self, text: str, force: bool = False):
        if not text:
            return
        now = time.time()
        if not force and (text == self.last_text or
                now - self.last_time < self.config.voice_min_gap or
                self.speaking):
            return
        self.last_text = text
        self.last_time = now
        threading.Thread(target=self._speak, args=(text,), daemon=True).start()

    def _speak(self, text: str):
        self.speaking = True
        try:
            lang = self.config.language
            if self._is_mac:
                voice = self.MAC_VOICES.get(lang, "Evan")
                subprocess.run(
                    ["say", "-v", voice, "-r", "170", text],
                    check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                )
            else:
                cmd = (
                    f'echo "{text}" | '
                    f'piper --model /home/pi/piper/{lang}.onnx '
                    f'--output_raw | aplay -r 22050 -f S16_LE -c 1'
                )
                subprocess.run(cmd, shell=True,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except Exception:
            pass
        finally:
            self.speaking = False


def render_game_hud(frame, cfg, stats, shot_clock, fps, plays, ble_connected=False):
    fh, fw = frame.shape[:2]
    F = cv2.FONT_HERSHEY_SIMPLEX
    FB = cv2.FONT_HERSHEY_DUPLEX
    overlay = frame.copy()
    cv2.rectangle(overlay, (0, 0), (fw, 52), (0, 0, 0), -1)
    cv2.addWeighted(overlay, cfg.hud_alpha, frame, 1 - cfg.hud_alpha, 0, frame)
    stats_text = f"PTS {stats.points}   AST {stats.assists}   REB {stats.rebounds}"
    cv2.putText(frame, stats_text, (12, 35), FB, 0.7, cfg.color_white, 2)
    sc_color = cfg.color_alert if shot_clock.remaining <= 5 else cfg.color_primary
    cv2.putText(frame, shot_clock.display, (fw - 55, 38), FB, 0.9, sc_color, 2)
    cv2.putText(frame, "VISIO", (fw - 58, fh - 10), F, 0.4, cfg.color_primary, 1)
    ble_dot = "BLE" if ble_connected else "..."
    cv2.putText(frame, f"FPS:{fps:.0f} PLAYS:{plays} {ble_dot}", (8, fh - 10), F, 0.35, cfg.color_dim, 1)
    return frame


def render_practice_hud(frame, cfg, stats, arc, fps):
    fh, fw = frame.shape[:2]
    F = cv2.FONT_HERSHEY_SIMPLEX
    FB = cv2.FONT_HERSHEY_DUPLEX
    overlay = frame.copy()
    cv2.rectangle(overlay, (0, 0), (fw, 52), (0, 0, 0), -1)
    cv2.addWeighted(overlay, cfg.hud_alpha, frame, 1 - cfg.hud_alpha, 0, frame)
    cv2.putText(frame, f"DRIB {stats.dribble_count}   SHOTS {stats.shot_count}",
                (12, 35), FB, 0.7, cfg.color_white, 2)
    if arc is not None:
        arc_color = cfg.color_ok if cfg.arc_ideal_min <= arc <= cfg.arc_ideal_max else cfg.color_alert
        cv2.putText(frame, f"{arc:.0f}deg", (fw - 90, 38), FB, 0.8, arc_color, 2)
    cv2.putText(frame, "VISIO", (fw - 58, fh - 10), F, 0.4, cfg.color_primary, 1)
    cv2.putText(frame, f"FPS:{fps:.0f}", (8, fh - 10), F, 0.35, cfg.color_dim, 1)
    return frame


def get_zone(px, py, fw, fh, cal=None):
    if cal and cal.calibrated:
        return cal.get_zone(px, py)
    nx, ny = px / fw, py / fh
    if ny > 0.85:
        return "BASELINE"
    if nx < 0.18 or nx > 0.82:
        return "CORNER"
    if ny < 0.35:
        return "THREE POINT"
    if 0.33 < nx < 0.67 and ny > 0.5:
        return "PAINT"
    return "MID RANGE"


def game_recommendation(frame, players, ball, teams, game_state, cal=None):
    if not players or ball is None:
        return ""
    if not game_state.is_live():
        return ""
    if game_state.is_long_pass():
        return "long_pass"

    fh, fw = frame.shape[:2]
    bx, by, _ = ball
    ball_zone = get_zone(bx, by, fw, fh, cal)
    ball_pos = (bx, by)

    team_a = [(p, t) for p, t in zip(players, teams) if t == 'A']
    team_b = [(p, t) for p, t in zip(players, teams) if t == 'B']

    if not team_a:
        return ""

    def center(p):
        return (p[0] + p[2] // 2, p[1] + p[3] // 2)

    def dist(a, b):
        return math.hypot(a[0] - b[0], a[1] - b[1])

    handler = min(team_a, key=lambda x: dist(center(x[0]), ball_pos), default=None)
    if not handler:
        return ""

    hx, hy = center(handler[0])
    defenders_on_handler = sum(1 for p, _ in team_b if dist(center(p), (hx, hy)) < 120)

    if by < fh * 0.28:
        ahead = sum(1 for p, _ in team_a if p[1] > by + 80)
        if ahead >= 2:
            return "fast_break"

    if defenders_on_handler >= 2:
        opens = []
        for p, _ in team_a:
            if p is handler[0]:
                continue
            px, py = center(p)
            nearest_def = min((dist(center(d), (px, py)) for d, _ in team_b), default=999)
            if nearest_def > 110:
                opens.append((nearest_def, get_zone(px, py, fw, fh, cal)))
        if opens:
            return "double_team"
        return "swing"

    if ball_zone == "PAINT":
        close_def = sum(1 for p, _ in team_b if dist(center(p), ball_pos) < 130)
        if close_def == 0:
            return "shoot_paint"
        if close_def == 1:
            return "post_move"

    if ball_zone == "MID RANGE":
        close_def = sum(1 for p, _ in team_b if dist(center(p), ball_pos) < 100)
        if close_def == 0:
            return "shoot_mid"

    if ball_zone == "CORNER":
        close_def = sum(1 for p, _ in team_b if dist(center(p), ball_pos) < 110)
        if close_def == 0:
            return "shoot_corner"

    sorted_def = sorted([p for p, _ in team_b], key=lambda b: b[0])
    for i in range(len(sorted_def) - 1):
        gap = sorted_def[i + 1][0] - (sorted_def[i][0] + sorted_def[i][2])
        if gap > 110:
            mid_x = sorted_def[i][0] + gap // 2
            return "drive_left" if mid_x < fw // 2 else "drive_right"

    opens = []
    for p, _ in team_a:
        px, py = center(p)
        if dist((px, py), ball_pos) < 60:
            continue
        nearest_def = min((dist(center(d), (px, py)) for d, _ in team_b), default=999)
        if nearest_def > 110:
            opens.append((nearest_def, get_zone(px, py, fw, fh, cal)))
    if opens:
        return "kick_out"

    return "move_ball"


def run_game_mode(cfg: VisioConfig):
    cam = CameraManager(cfg)
    if not cam.connect():
        return
    cam.start()
    det = PlayerDetector(cfg)
    ball_tracker = BallTracker(cfg)
    saver = PlaySaver(cfg)
    voice = VoiceCoach(cfg)
    stats = PlayerStats()
    shot_clock = ShotClock(cfg.shot_clock_seconds)
    game_state = GameState(cfg)

    # BLE server
    ble = VisioGATTServer()
    if cfg.ble_enabled:
        ble.start()

    print("[VISIO] Starting court calibration...")
    cal_frame = cam.get_frame()
    if not cfg.headless:
        cal = run_calibration(cal_frame)
        if cal.calibrated:
            print("[VISIO] Court calibrated successfully.")
        else:
            print("[VISIO] Calibration skipped — using default zones.")
    else:
        cal = CourtCalibration(calibrated=False,
                               frame_w=cal_frame.shape[1],
                               frame_h=cal_frame.shape[0])

    players, teams = [], []
    fn = 0
    fps_timer = time.time()
    fps_display = 0.0
    prev_gs = GameState.LIVE
    shot_clock.start()

    print("[VISIO] GAME MODE — Q quit | M save | S streetball")

    while True:
        frame = cam.get_frame()
        fn += 1
        fh, fw = frame.shape[:2]
        saver.push(frame)

        if fn % cfg.detection_interval == 0:
            players = det.detect(frame)
            teams = det.classify_teams(frame, players)

        ball = ball_tracker.detect(frame)
        dribble = ball_tracker.is_dribble(fh)

        expired = shot_clock.tick()
        if expired:
            voice.say(CUES[cfg.language]["dead_ball"], force=True)
            shot_clock.reset()
            shot_clock.start()

        gs = game_state.update(ball, fw, fh)
        game_live = game_state.is_live()

        if gs != prev_gs:
            if gs == GameState.LIVE:
                voice.say(CUES[cfg.language]["ball_live"], force=True)
                shot_clock.reset()
                shot_clock.start()
            elif gs == GameState.OOB:
                voice.say(CUES[cfg.language]["out_of_bounds"], force=True)
                shot_clock.reset()
                shot_clock.stop()
            elif gs == GameState.DEAD:
                voice.say(CUES[cfg.language]["dead_ball"], force=True)
                shot_clock.reset()
                shot_clock.stop()
        prev_gs = gs

        stat_event = stats.update(
            ball, fh, fw,
            ball_tracker.shot_active,
            dribble,
            ball_tracker.velocity
        )
        if stat_event == "scored":
            voice.say(CUES[cfg.language]["you_scored"], force=True)
            saver.trigger("SCORED")
            shot_clock.reset()
            shot_clock.start()
        elif stat_event == "rebound":
            voice.say(CUES[cfg.language]["you_rebounded"], force=True)

        if game_live:
            cue_key = game_recommendation(frame, players, ball, teams, game_state, cal)
            if cue_key:
                voice.say(CUES[cfg.language].get(cue_key, ""))

        # Push stats over BLE every 10 frames
        if fn % 10 == 0 and cfg.ble_enabled:
            ble.push_stats(
                stats.points, stats.assists, stats.rebounds,
                shot_clock.remaining, game_live
            )

        # Update WebSocket state
        ws_state["pts"] = stats.points
        ws_state["ast"] = stats.assists
        ws_state["reb"] = stats.rebounds
        ws_state["shot_clock"] = shot_clock.remaining
        ws_state["mode"] = "game"

        if fn % 30 == 0:
            elapsed = time.time() - fps_timer
            fps_display = 30 / elapsed if elapsed > 0 else 0
            fps_timer = time.time()

        frame = render_game_hud(frame, cfg, stats, shot_clock,
                                fps_display, saver.saved_count, ble.connected)

        if not cfg.headless:
            cv2.imshow("VISIO — GAME MODE", frame)
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            if key == ord('m'):
                saver.manual_save()
            if key == ord('r'):
                shot_clock.reset()
                shot_clock.start()
            if key == ord('s'):
                cfg.streetball_mode = not cfg.streetball_mode
                print(f"[VISIO] Streetball {'ON' if cfg.streetball_mode else 'OFF'}")
        else:
            time.sleep(0.001)

    ble.stop()
    cam.stop()
    cv2.destroyAllWindows()


def run_practice_mode(cfg: VisioConfig):
    cam = CameraManager(cfg)
    if not cam.connect():
        return
    cam.start()
    ball_tracker = BallTracker(cfg)
    voice = VoiceCoach(cfg)
    stats = PlayerStats()

    fn = 0
    fps_timer = time.time()
    fps_display = 0.0
    current_arc: Optional[float] = None
    last_shot_active = False

    print("[VISIO] PRACTICE MODE — Q quit")

    while True:
        frame = cam.get_frame()
        fn += 1
        fh = frame.shape[0]

        ball = ball_tracker.detect(frame)
        dribble = ball_tracker.is_dribble(fh)

        if dribble:
            stats.dribble_count += 1

        arc = ball_tracker.get_shot_arc()
        if arc is not None:
            current_arc = arc
            lang = cfg.language
            if arc < cfg.arc_ideal_min:
                voice.say(CUES[lang]["arc_low"])
            elif arc > cfg.arc_ideal_max:
                voice.say(CUES[lang]["arc_high"])
            else:
                voice.say(CUES[lang]["perfect_arc"])

        if ball_tracker.shot_active and not last_shot_active:
            stats.shot_count += 1
        last_shot_active = ball_tracker.shot_active

        if fn % 30 == 0:
            elapsed = time.time() - fps_timer
            fps_display = 30 / elapsed if elapsed > 0 else 0
            fps_timer = time.time()

        frame = render_practice_hud(frame, cfg, stats, current_arc, fps_display)

        if not cfg.headless:
            cv2.imshow("VISIO — PRACTICE MODE", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        else:
            time.sleep(0.001)

    cam.stop()
    cv2.destroyAllWindows()


def run_camera_test(cfg: VisioConfig):
    cam = CameraManager(cfg)
    if not cam.connect():
        return
    cam.start()
    print("[VISIO] CAMERA TEST — Q quit")
    fn = 0
    fps_timer = time.time()
    fps_display = 0.0

    while True:
        frame = cam.get_frame()
        fn += 1
        if fn % 30 == 0:
            elapsed = time.time() - fps_timer
            fps_display = 30 / elapsed if elapsed > 0 else 0
            fps_timer = time.time()
        cv2.putText(frame, "VISIO CAMERA TEST — OK", (20, 40),
                    cv2.FONT_HERSHEY_DUPLEX, 1.0, (0, 165, 255), 2)
        cv2.putText(frame, f"FPS: {fps_display:.0f}", (20, 80),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        cv2.putText(frame, f"CAM INDEX: {cfg.camera_index}", (20, 110),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        cv2.putText(frame, f"RES: {frame.shape[1]}x{frame.shape[0]}", (20, 140),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        if not cfg.headless:
            cv2.imshow("VISIO — CAMERA TEST", frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break
        else:
            time.sleep(0.001)

    cam.stop()
    cv2.destroyAllWindows()


def print_spec():
    print("""
╔══════════════════════════════════════════════════════════╗
║         VISIO SMART GOGGLES — HARDWARE SPEC SHEET        ║
║         Patent Pending | App. 64/020,415                 ║
╠══════════════════════════════════════════════════════════╣
║ CAMERAS     2x 1920×1080 @ 240fps | 110° FOV             ║
║             Global shutter | MIPI CSI-2                  ║
║ PROCESSOR   ARM Cortex-A55 quad-core 1.8GHz              ║
║             4+ TOPS NPU | 4GB LPDDR4X | 16GB eMMC        ║
║ DISPLAY     Micro-OLED waveguide | 3000 nits             ║
║             Upper 8–12mm of lens                         ║
║ AUDIO       Bone conduction ×2 | Dual MEMS mic           ║
║ CONNECT     Bluetooth 5.0 | Wi-Fi 802.11ac | USB-C       ║
║ POWER       2500mAh | 4–6hr game life | 18W fast charge  ║
║ FRAME       Matte black wraparound                       ║
║             Orange woven nylon strap                     ║
║             Magnetic Rx lens insert                      ║
║ CERTS       FCC | CE | RoHS | IP54                       ║
╚══════════════════════════════════════════════════════════╝
""")

# ── WebSocket Server ─────────────────────────────────────────────────────────
import asyncio
import websockets
import json
import threading

ws_state = {
    "pts": 0, "ast": 0, "reb": 0,
    "fg": 0, "fg_att": 0, "three": 0, "three_att": 0, "to": 0,
    "shot_clock": 24.0,
    "cues": [],
    "mode": "idle"
}

async def ws_handler(websocket):
    while True:
        try:
            await websocket.send(json.dumps(ws_state))
            await asyncio.sleep(0.1)
        except:
            break

async def ws_main():
    async with websockets.serve(ws_handler, "0.0.0.0", 8765):
        await asyncio.Future()

def start_ws_server():
    asyncio.run(ws_main())

def ws_server_thread():
    t = threading.Thread(target=start_ws_server, daemon=True)
    t.start()
if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="VISIO — AI Basketball Smart Goggles | Patent Pending 64/020,415"
    )
    parser.add_argument("--mode", choices=["game", "practice", "test", "spec"], default="test")
    parser.add_argument("--camera", type=int, default=None)
    parser.add_argument("--lang", type=str, default="en", choices=list(CUES.keys()))
    parser.add_argument("--headless", action="store_true")
    parser.add_argument("--streetball", action="store_true")
    parser.add_argument("--no-ble", action="store_true", help="Disable BLE server")
    args = parser.parse_args()

    cfg = VisioConfig()
    ws_server_thread()
    cfg.language = args.lang
    cfg.headless = args.headless
    cfg.streetball_mode = args.streetball
    cfg.ble_enabled = not args.no_ble
    if args.camera is not None:
        cfg.camera_index = args.camera

    if args.mode == "game":
        run_game_mode(cfg)
    elif args.mode == "practice":
        run_practice_mode(cfg)
    elif args.mode == "test":
        run_camera_test(cfg)
    elif args.mode == "spec":
        print_spec()
