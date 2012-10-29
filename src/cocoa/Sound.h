/****************************************************************************
**
** Copyright (C) 2012 Lorem Ipsum Mediengesellschaft m.b.H.
**
** GNU General Public License
** This file may be used under the terms of the GNU General Public License
** version 3 as published by the Free Software Foundation and
** appearing in the file LICENSE.GPL included in the packaging of this file.
**
****************************************************************************/

#ifndef SOUND_INCLUDE_H
#define SOUND_INCLUDE_H

#include <QString>

/**
 * Singleton that handles sounds
 */
class Sound
{
public:
    /**
     * Get the instance of the object
     * @return Instance of the object
     */
    static Sound &getInstance();

    /**
     * Start ring sound
     */
    void startRing();

    /**
     * Start dial sound
     */
    void startDial();

    /**
     * Stop sounds
     */
    void stop();
    
    void setSoundDevice(const int device);
    
    QString ringFilename;
    
private:
    int device_;
    NSSound *sound_;

    Sound();
    ~Sound();
};

#endif // SOUND_INCLUDE_H
