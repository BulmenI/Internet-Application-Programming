import cv2
import numpy as np
import datetime

# Загрузка каскадного классификатора для лиц
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Загрузка HOG дескриптора для туловища
hog = cv2.HOGDescriptor()
hog.setSVMDetector(cv2.HOGDescriptor_getDefaultPeopleDetector())

video_capture = cv2.VideoCapture(0)

log_filename = datetime.datetime.now().strftime("%Y-%m-%d") + ".log"
danger_activated = False

human_count = 0
show_count = False 
display_count = 0 
danger_activated = False 

log_filename = datetime.datetime.now().strftime("%Y-%m-%d") + ".log"

def write_log(filename, message):
    with open(filename, "a") as log_file:
        log_file.write(f"{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')} - {message}\n")

def read_arduino_log(filename):
    try:
      with open(filename, "r") as logfile:
        lines = logfile.readlines()
        return lines[-1].strip() # Возвращаем последнюю строку лога
    except FileNotFoundError:
        return None
    except IndexError:
        return None
    
while True:
    # Чтение кадра и преобразование в оттенки серого
    ret, frame = video_capture.read()
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # Обнаружение лиц
    faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))

    # Обнаружение туловища
    (rects, weights) = hog.detectMultiScale(frame, winStride=(4, 4), padding=(8, 8), scale=1.05)

    detections = []
    if len(faces) > 0:
        detections.extend(faces)
    if len(rects) > 0:
        detections.extend(np.array([[x, y, w, h] for (x, y, w, h) in rects]))

    # Обработка обнаружений
    if len(detections) > 0:
        is_human = False 
        for (x, y, w, h) in detections:
            aspect_ratio = float(w) / h
            if 0.2 < aspect_ratio < 0.8: 
                is_human = True
                break 

        if is_human: # Рисуем прямоугольник и увеличиваем счетчики, только если это человек
            if not show_count:
                human_count += 1
                show_count = True
                print(f"human count: {human_count}")
            display_count += 1 
            if display_count >= 1000: 
                danger_activated = True
            if not show_count:
                write_log(log_filename, f"Human detected. Total humans: {human_count}, Display Count: {display_count}")
            for (x, y, w, h) in detections:
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)


            # Отображение счетчика на кадре
            cv2.putText(frame, f"Display Count: {display_count}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
        



        else:
            print("not a human")

    else:
        show_count = False
        print("not a human")

    # Проверка лога Arduino
    arduino_log_entry = read_arduino_log(log_filename)
    if arduino_log_entry:
        if "ALERT" in arduino_log_entry:
            danger_activated = True
        else:
            danger_activated = False
            
    if danger_activated:
        cv2.putText(frame, "ОПАСНО", (100, 200), cv2.FONT_HERSHEY_SIMPLEX, 3, (0, 0, 255), 5)



    cv2.imshow('Video', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        write_log(log_filename, f"Program closed. Final humans count: {human_count}, Final Display Count: {display_count}") # Лог при закрытии
        break

video_capture.release()
cv2.destroyAllWindows()